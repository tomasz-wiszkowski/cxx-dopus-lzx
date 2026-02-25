// opusPlugin.cpp : Defines the exported functions for the DLL application.
//

#include <strsafe.h>

#include <cwctype>
#include <memory>

#include "dopus_wstring_view_span.hh"
#include "stdafx.h"
#include "text_utils.hh"

// unlzx
#include "error.hh"

DOpusPluginHelperFunction DOpus;

void Plugin::ReconstructDirStructure() {
  mRoot = std::make_shared<DirEnt>();
  mCurrentDir = mRoot.get();

  if (!mArchive)
    return;

  mFlatMap = std::make_shared<std::map<std::string, LzxEntry>>(mArchive->list_archive());
  for (auto& [name, entry] : *mFlatMap) {
    std::filesystem::path path(name);

    DirEnt* insertion_point = mRoot.get();
    for (auto segment : path) {
      insertion_point = &insertion_point->children_[segment.string()];
    }
    insertion_point->file_ = &entry;
  }
}

std::optional<std::filesystem::path> Plugin::LoadFile(std::filesystem::path path) {
  path = sanitize(std::move(path));
  SetError(0);

  if (!mPath.empty() && is_subpath(mPath, path)) {
    // Path is already loaded, no need to check again.
    return std::filesystem::relative(path, mPath);
  }

  // Loading new file. Should we cache this?
  mPath.clear();
  mArchive.reset();

  SetError(ERROR_FILE_NOT_FOUND);

  // Walk the pAdfPath up until we find the valid file.
  std::filesystem::path real_file_path = path;
  while (!real_file_path.empty()) {
    if (std::filesystem::exists(real_file_path))
      break;
    real_file_path = real_file_path.parent_path();
  }

  if (real_file_path.empty())
    return {};

  // Get extension and check if it's supported.
  auto extension = real_file_path.extension().wstring();
  std::ranges::transform(extension, extension.begin(), std::towlower);
  if (extension != L".lzx")
    return {};

  auto utf_file_path = real_file_path.string();

  mArchive = std::make_shared<Unlzx>();
  Status result = mArchive->open_archive(utf_file_path.c_str());
  if (result != Status::Ok)
    return {};

  SetError(0);
  mPath = real_file_path;
  ReconstructDirStructure();
  return std::filesystem::relative(path, mPath);
}

bool Plugin::ChangeDir(std::filesystem::path dir) {
  dir = sanitize(std::move(dir));
  auto maybe_path = LoadFile(std::move(dir));
  if (!maybe_path)
    return false;

  mCurrentDir = mRoot.get();
  if (*maybe_path == ".")
    return true;

  for (auto segment : *maybe_path) {
    auto iter = mCurrentDir->children_.find(segment.string());
    if (iter == mCurrentDir->children_.end())
      return false;
    mCurrentDir = &iter->second;
  }
  return true;
}

LPVFSFILEDATAHEADER Plugin::GetfileInformation(std::filesystem::path path, HANDLE heap) {
  SetError(ERROR_FILE_NOT_FOUND);
  if (!ChangeDir(path.parent_path()))
    return nullptr;
  auto filename = path.filename().wstring();

  auto iter = mCurrentDir->children_.find(path.filename().string());
  if (iter == mCurrentDir->children_.end())
    return {};

  SetError(0);
  return GetVFSforEntry(iter->first, iter->second, heap);
}

bool Plugin::GetFileAttr(std::filesystem::path path, LPDWORD pAttr) {
  path = sanitize(std::move(path));
  if (!ChangeDir(path))
    return false;

  if (mCurrentDir->file_) {
    *pAttr = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_COMPRESSED;
  } else {
    *pAttr = FILE_ATTRIBUTE_DIRECTORY;
  }
  return true;
}

bool Plugin::GetFileSize(std::filesystem::path path, PluginFile* file, uint64_t* piFileSize) {
  path = sanitize(std::move(path));
  if (!ChangeDir(path))
    return false;

  if (!mCurrentDir->file_)
    return false;

  *piFileSize = mCurrentDir->file_->unpack_size();
  return true;
}

LPVFSFILEDATAHEADER Plugin::GetVFSforEntry(const std::string& name, const DirEnt& entry, HANDLE heap) {
  LPVFSFILEDATAHEADER node;

  node = static_cast<LPVFSFILEDATAHEADER>(HeapAlloc(heap, 0, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATA)));
  if (!node)
    return nullptr;

  LPVFSFILEDATAW details = reinterpret_cast<LPVFSFILEDATAW>(node + 1);

  node->cbSize = sizeof(VFSFILEDATAHEADER);
  node->lpNext = nullptr;
  node->iNumItems = 1;
  node->cbFileDataSize = sizeof(VFSFILEDATA);

  details->dwFlags = 0;
  details->lpszComment = nullptr;
  details->iNumColumns = 0;
  details->lpvfsColumnData = nullptr;

  GetWfdForEntry(name, entry, &details->wfdData);

  return node;
}

void Plugin::GetWfdForEntry(const std::string& name, const DirEnt& entry, LPWIN32_FIND_DATAW data) {
  auto wname = utf8_to_wstring(name);
  StringCchCopyW(data->cFileName, MAX_PATH, wname.c_str());

  data->nFileSizeHigh = 0;
  if (entry.file_) {
    data->nFileSizeLow = entry.file_->unpack_size();
    data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_COMPRESSED;
  } else {
    data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
  }

  data->dwReserved0 = 0;
  data->dwReserved1 = 0;

  data->ftCreationTime = {};
  data->ftLastAccessTime = {};
  // data->ftLastWriteTime = GetFileTime(entry);
}

bool Plugin::ReadDirectory(LPVFSREADDIRDATAW lpRDD) {
  // Free directory if lister is closing (otherwise ignore free command)
  if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE)
    return true;

  if (lpRDD->vfsReadOp == VFSREAD_FREEDIR)
    return true;

  if (!ChangeDir(lpRDD->lpszPath))
    return false;

  if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR)
    return true;

  auto& directory = mCurrentDir->children_;
  LPVFSFILEDATAHEADER lpLastHeader = nullptr;

  for (const auto& [name, entry] : directory) {
    auto* node = GetVFSforEntry(name, entry, lpRDD->hMemHeap);
    if (!node)
      break;

    if (lpLastHeader) {
      lpLastHeader->lpNext = node;
    } else {
      lpRDD->lpFileData = node;
    }
    lpLastHeader = node;
  }

  return true;
}

bool Plugin::ReadFile(PluginFile* file, std::span<uint8_t> buffer, LPDWORD read_size) {
  SetError(0);
  *read_size = 0;

  if (file->offset_ >= file->file_->unpack_size())
    return false;

  // Locate segment to read from
  size_t read_offset{file->offset_};
  size_t segment_size{};

  auto segment_iter = file->file_->segments().begin();
  for (; segment_iter != file->file_->segments().end(); ++segment_iter) {
    segment_size = segment_iter->decompressed_length();
    // Locate first segment that has any relevant data.
    if (read_offset < segment_size)
      break;

    read_offset -= segment_size;
  }

  if (segment_iter == file->file_->segments().end())
    return false;

  std::span<const uint8_t> data = segment_iter->data();
  if (data.empty()) {
    SetError(ERROR_READ_FAULT);
    return false;
  }

  *read_size = min(segment_size - read_offset, buffer.size());
  ::memcpy(buffer.data(), &data[read_offset], *read_size);
  file->offset_ += *read_size;

  return true;
}

bool Plugin::WriteFile(PluginFile* pFile, std::span<uint8_t> buffer, LPDWORD pWriteSize) {
  /* Not implemented */
  return {};
}

PluginFile* Plugin::OpenFile(std::filesystem::path path, bool for_writing) {
  path = sanitize(std::move(path));
  if (!ChangeDir(path.parent_path()))
    return {};

  if (for_writing)
    return {};

  auto iter = mCurrentDir->children_.find(path.filename().string());
  if (iter == mCurrentDir->children_.end())
    return {};
  if (!iter->second.file_)
    return {};

  auto result = new PluginFile();
  result->file_ = iter->second.file_;
  return result;
}

void Plugin::CloseFile(PluginFile* file) {
  delete file;
}

int Plugin::ContextVerb(LPVFSCONTEXTVERBDATAW lpVerbData) {
  std::filesystem::path full_path = sanitize(lpVerbData->lpszPath);
  if (!ChangeDir(full_path.parent_path()))
    return VFSCVRES_FAIL;

  auto item = mCurrentDir->children_.find(full_path.filename().string());

  if (item == mCurrentDir->children_.end())
    return VFSCVRES_FAIL;
  if (!item->second.file_)
    return VFSCVRES_DEFAULT;

  if (item->second.extracted_path_.empty())
    return VFSCVRES_EXTRACT;

  // I honestly have no clue what the expectation here is; this kind of works on the second double click but that ain't
  // right.
  StringCchCopyW(lpVerbData->lpszNewPath, MAX_PATH, item->second.extracted_path_.wstring().c_str());
  return VFSCVRES_CHANGE;
}

PluginFindData* Plugin::FindFirst(std::filesystem::path path, LPWIN32_FIND_DATA lpwfdData, HANDLE hAbortEvent) {
  /* Not implemented */
  return {};
}

bool Plugin::FindNext(PluginFindData* lpRAF, LPWIN32_FIND_DATA lpwfdData) {
  /* Not implemented */
  return {};
}

void Plugin::FindClose(PluginFindData* pFindData) {
  /* Not implemented */
}

bool Plugin::Extract(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path) {
  /* Not implemented */
  return {};
}

bool Plugin::ExtractPath(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path) {
  /* Not implemented */
  return {};
}

bool Plugin::ExtractFile(LPVOID func_data, const EntryType& pEntry, std::filesystem::path target_path) {
  /* Not implemented */
  return {};
}

size_t Plugin::GetAvailableSize() {
  /* Not implemented */
  return {};
}

size_t Plugin::GetTotalSize() {
  /* Not implemented */
  return {};
}

Guard<HANDLE> Plugin::SetAbortHandle(HANDLE& hAbortEvent) {
  return Guard<HANDLE>(mAbortEvent, hAbortEvent);
}

bool Plugin::ShouldAbort() const {
  return mAbortEvent && WaitForSingleObject(mAbortEvent, 0) == WAIT_OBJECT_0;
}

uint32_t Plugin::BatchOperation(std::filesystem::path path, LPVFSBATCHDATAW lpBatchData) {
  /* Not implemented */
  return {};
}

bool Plugin::PropGet(vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
  switch (propId) {
    case VFSPROP_CANSHOWSUBFOLDERS:
    case VFSPROP_ISEXTRACTABLE:
    case VFSPROP_SHOWTHUMBNAILS:
      *reinterpret_cast<LPBOOL>(lpPropData) = true;
      break;

    case VFSPROP_ALLOWTOOLTIPGETSIZES:
    case VFSPROP_CANDELETESECURE:
    case VFSPROP_CANDELETETOTRASH:
    case VFSPROP_SHOWFILEINFO:
    case VFSPROP_SUPPORTFILEHASH:
    case VFSPROP_SUPPORTPATHCOMPLETION:
    case VFSPROP_USEFULLRENAME:
      *reinterpret_cast<LPBOOL>(lpPropData) = false;
      break;

    case VFSPROP_SHOWPICTURESDIRECTLY:
      *reinterpret_cast<LPDWORD>(lpPropData) = false;  // I don't think this works like the documentation says.
      break;

    case VFSPROP_SHOWFULLPROGRESSBAR:  // No progress bar even when copying.
      *reinterpret_cast<LPDWORD>(lpPropData) = false;
      break;

    case VFSPROP_DRAGEFFECTS:
      *reinterpret_cast<LPDWORD>(lpPropData) = DROPEFFECT_COPY;
      break;

    case VFSPROP_BATCHOPERATION:
      *reinterpret_cast<LPDWORD>(lpPropData) = VFSBATCHRES_CALLFOREACH;
      break;

    case VFSPROP_GETVALIDACTIONS:
      *reinterpret_cast<LPDWORD>(lpPropData) = /* SFGAO_*/ 0;
      break;

    case VFSPROP_COPYBUFFERSIZE:
      *reinterpret_cast<LPDWORD>(lpPropData) = 64 << 20;
      break;

    case VFSPROP_FUNCAVAILABILITY:
      *reinterpret_cast<LPDWORD>(lpPropData) &=
          ~(VFSFUNCAVAIL_MOVE |
            VFSFUNCAVAIL_DELETE
            // | VFSFUNCAVAIL_GETSIZES
            | VFSFUNCAVAIL_MAKEDIR | VFSFUNCAVAIL_PRINT | VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_RENAME |
            VFSFUNCAVAIL_SETATTR |
            VFSFUNCAVAIL_SHORTCUT
            //| VFSFUNCAVAIL_SELECTALL
            //| VFSFUNCAVAIL_SELECTNONE
            //| VFSFUNCAVAIL_SELECTINVERT
            // | VFSFUNCAVAIL_VIEWLARGEICONS | VFSFUNCAVAIL_VIEWSMALLICONS | VFSFUNCAVAIL_VIEWLIST |
            // VFSFUNCAVAIL_VIEWDETAILS |
            // VFSFUNCAVAIL_VIEWTHUMBNAIL
            // | VFSFUNCAVAIL_CLIPCOPY
            | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE | VFSFUNCAVAIL_CLIPPASTESHORTCUT |
            VFSFUNCAVAIL_UNDO
            //| VFSFUNCAVAIL_SHOW
            | VFSFUNCAVAIL_DUPLICATE |
            VFSFUNCAVAIL_SPLITJOIN
            //| VFSFUNCAVAIL_SELECTRESELECT
            //| VFSFUNCAVAIL_SELECTALLFILES
            //| VFSFUNCAVAIL_SELECTALLDIRS
            //| VFSFUNCAVAIL_PLAY
            | VFSFUNCAVAIL_SETTIME | VFSFUNCAVAIL_VIEWTILE | VFSFUNCAVAIL_SETCOMMENT);
      break;

      // VFSPROP_GETFOLDERICON -> return icon file?
    default:
      return false;
  }

  return true;
}

bool Plugin::ExtractEntries(LPVOID func_data, dopus::wstring_view_span entry_names, std::filesystem::path target_path) {
  SetError(0);
  for (auto name : entry_names) {
    std::filesystem::path source_path = sanitize(name);
    // Directory not found.
    if (!ChangeDir(source_path.parent_path()))
      continue;

    // File not found.
    auto file_iter = mCurrentDir->children_.find(source_path.filename().string());
    if (file_iter == mCurrentDir->children_.end())
      continue;

    // Not a file.
    if (!file_iter->second.file_)
      continue;

    auto relative_path = std::filesystem::relative(source_path, mPath);
    auto target_file = target_path / relative_path;
    std::filesystem::create_directories(target_file.parent_path());
    file_iter->second.extracted_path_ = target_file;
    std::ofstream target(target_file, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
    for (auto segment : file_iter->second.file_->segments()) {
      if (target.bad())
        break;

      auto data = segment.data();

      // Decompress failure.
      if (segment.status() != Status::Ok)
        break;

      // Write failure.
      target.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    target.close();
    DOpus.AddFunctionFileChange(func_data, /* fIsDest= */ false, OPUSFILECHANGE_CREATE, target_file.c_str());
  }

  return true;
}

void Plugin::SetError(int error) {
  mLastError = error;
  ::SetLastError(error);
}

#include <strsafe.h>

#include <cwctype>
#include <memory>

#include "dopus_wstring_view_span.hh"
#include "stdafx.h"
#include "text_utils.hh"

// unlzx
#include "error.hh"

DOpusPluginHelperFunction DOpus;

namespace {
/// @brief  Creates a FILETIME structure representing the given UTC time components.
std::optional<FILETIME> MakeFileTime(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second) {
  SYSTEMTIME local{.wYear = year,
                   .wMonth = month,
                   .wDay = day,
                   .wHour = hour,
                   .wMinute = minute,
                   .wSecond = second,
                   .wMilliseconds = 0};

  SYSTEMTIME utc{};
  if (!TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc))
    return {};

  FILETIME ft{};
  if (!SystemTimeToFileTime(&utc, &ft))
    return {};

  return ft;
}
}  // namespace

// --- Directory Structure & Navigation ---

void Plugin::ReconstructDirStructure() {
  mRoot = std::make_shared<DirEnt>();
  mCurrentDir = mRoot.get();

  if (!mArchive)
    return;

  mFlatMap = std::make_shared<std::map<std::string, LzxEntry>>(mArchive->list_archive());
  for (auto& [name, entry] : *mFlatMap) {
    auto path = Path::from_foreign_path(latin1_to_wstring(name));

    DirEnt* insertion_point = mRoot.get();

    // Skip the empty filename component if path refers to a directory.
    for (auto segment : path.path_components()) {
      insertion_point = &insertion_point->children_[std::wstring(segment)];
    }
    insertion_point->entry_ = &entry;

    // LZX quirk: some entries may have a trailing `/` indicating a directory, to preserve
    // some of the filesystem metadata.
    insertion_point->is_file_ = !name.ends_with('/');
  }
}

bool Plugin::ChangeDir(Path dir) {
  auto maybe_path = LoadFile(std::move(dir));
  if (!maybe_path)
    return false;

  mCurrentDir = mRoot.get();
  // Normally C++ should substitute `==` with `(<=>) == 0` automatically, but it seems to not
  // be doing that for some reason, so we do it manually here.
  if ((*maybe_path <=> L".") == 0)
    return true;

  for (auto segment : maybe_path->path_components()) {
    auto iter = mCurrentDir->children_.find(std::wstring_view(segment));
    if (iter == mCurrentDir->children_.end())
      return false;
    mCurrentDir = &iter->second;
  }
  return true;
}

// --- Entry Information ---

LPVFSFILEDATAHEADER Plugin::GetVFSforEntry(std::wstring_view name, const DirEnt& item, HANDLE heap) {
  LPVFSFILEDATAHEADER node;

  node = static_cast<LPVFSFILEDATAHEADER>(HeapAlloc(heap, 0, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATA)));
  if (!node)
    return nullptr;

  LPVFSFILEDATAW details = reinterpret_cast<LPVFSFILEDATAW>(node + 1);

  node->cbSize = sizeof(VFSFILEDATAHEADER);
  node->lpNext = nullptr;
  node->iNumItems = 1;
  node->cbFileDataSize = sizeof(VFSFILEDATA);

  details->lpszComment = nullptr;
  details->dwFlags = 0;
  details->iNumColumns = 0;
  details->lpvfsColumnData = nullptr;

  GetWfdForEntry(name, item, &details->wfdData);

  if (item.entry_ && !item.entry_->comment().empty()) {
    auto wcomment = latin1_to_wstring(item.entry_->comment());
    details->lpszComment = static_cast<LPWSTR>(HeapAlloc(heap, 0, (wcomment.size() + 1) * sizeof(wchar_t)));
    memcpy(details->lpszComment, wcomment.c_str(), (wcomment.size() + 1) * sizeof(wchar_t));
  }

  return node;
}

void Plugin::GetWfdForEntry(std::wstring_view name, const DirEnt& item, LPWIN32_FIND_DATAW data) {
  StringCchCopyW(data->cFileName, MAX_PATH, name.data());

  data->nFileSizeHigh = 0;
  if (item.is_file_) {
    data->nFileSizeLow = item.entry_->unpack_size();
    data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_COMPRESSED;
  } else {
    data->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
  }

  if (item.entry_) {
    auto flags = item.entry_->attributes();
    if (!flags.writable && !flags.deletable)
      data->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    if (flags.hidden)
      data->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    if (flags.archived)
      data->dwFileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    if (flags.script)
      data->dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;

    auto datestamp = item.entry_->datestamp();
    data->ftLastAccessTime = MakeFileTime(datestamp.year(), datestamp.month() + 1, datestamp.day(), datestamp.hour(),
                                          datestamp.minute(), datestamp.second())
                                 .value_or(FILETIME{});
    data->ftCreationTime = data->ftLastAccessTime;
    data->ftLastWriteTime = data->ftLastAccessTime;
  }

  data->dwReserved0 = 0;
  data->dwReserved1 = 0;

  data->ftCreationTime = {};
  data->ftLastAccessTime = {};
  // data->ftLastWriteTime = GetFileTime(entry);
}

// --- State Management & Helpers ---

Guard<HANDLE> Plugin::SetAbortHandle(HANDLE& hAbortEvent) {
  return Guard<HANDLE>(mAbortEvent, hAbortEvent);
}

bool Plugin::ShouldAbort() const {
  return mAbortEvent && WaitForSingleObject(mAbortEvent, 0) == WAIT_OBJECT_0;
}

void Plugin::SetError(int error) {
  mLastError = error;
  ::SetLastError(error);
}

// --- Initialization & Archive Info ---

std::optional<Path> Plugin::LoadFile(Path path) {
  SetError(0);

  if (!mPath.empty()) {
    auto maybe_subpath = path.relative_to(mPath);
    if (maybe_subpath)
      return maybe_subpath;
  }

  // Loading new file. Should we cache this?
  mPath.clear();
  mArchive.reset();

  SetError(ERROR_FILE_NOT_FOUND);

  // Walk the pAdfPath up until we find the valid file.
  Path real_file_path = path;
  while (!real_file_path.empty()) {
    if (real_file_path.exists())
      break;
    real_file_path = real_file_path.parent_path();
  }

  if (real_file_path.empty())
    return {};

  // Get extension and check if it's supported.
  std::wstring extension = real_file_path.extension().wstring();
  std::ranges::transform(extension, extension.begin(), std::towlower);
  if (extension != L".lzx")
    return {};

  auto utf_file_path = wstring_to_utf8(real_file_path.wstring());

  mArchive = std::make_shared<Unlzx>();
  Status result = mArchive->open_archive(utf_file_path.c_str());
  if (result != Status::Ok)
    return {};

  SetError(0);
  mPath = real_file_path;
  ReconstructDirStructure();
  return path.relative_to(mPath);
}

size_t Plugin::GetAvailableSize() {
  /* Not implemented */
  return {};
}

size_t Plugin::GetTotalSize() {
  /* Not implemented */
  return {};
}

// --- Directory Reading ---

bool Plugin::ReadDirectory(LPVFSREADDIRDATAW lpRDD) {
  // Free directory if lister is closing (otherwise ignore free command)
  if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE)
    return true;

  if (lpRDD->vfsReadOp == VFSREAD_FREEDIR)
    return true;

  if (!ChangeDir(Path(lpRDD->lpszPath)))
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

// --- File I/O ---

PluginFile* Plugin::OpenFile(Path path, bool for_writing) {
  if (!ChangeDir(path.parent_path()))
    return {};

  if (for_writing)
    return {};

  auto iter = mCurrentDir->children_.find(path.filename().wstring());
  if (iter == mCurrentDir->children_.end())
    return {};
  if (!iter->second.is_file_)
    return {};

  auto result = new PluginFile();
  result->file_ = iter->second.entry_;
  return result;
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

void Plugin::CloseFile(PluginFile* file) {
  delete file;
}

// --- File Enumeration ---

struct PluginFindData {
  std::map<std::wstring, Plugin::DirEnt>::iterator current;
  std::map<std::wstring, Plugin::DirEnt>::iterator end;
};

PluginFindData* Plugin::FindFirst(Path path, LPWIN32_FIND_DATA lpwfdData, HANDLE hAbortEvent) {
  SetError(0);

  // We assume the pattern is always '*' (or similar), so we just list the directory.
  if (!ChangeDir(path.parent_path())) {
    SetError(ERROR_PATH_NOT_FOUND);
    return nullptr;
  }

  auto* find_data = new PluginFindData();
  find_data->current = mCurrentDir->children_.begin();
  find_data->end = mCurrentDir->children_.end();

  if (FindNext(find_data, lpwfdData)) {
    return find_data;
  }

  delete find_data;
  return nullptr;
}

bool Plugin::FindNext(PluginFindData* lpRAF, LPWIN32_FIND_DATA lpwfdData) {
  SetError(0);
  if (!lpRAF) {
    SetError(ERROR_INVALID_HANDLE);
    return false;
  }

  if (lpRAF->current != lpRAF->end) {
    auto& [name, entry] = *lpRAF->current;
    lpRAF->current++;

    GetWfdForEntry(name, entry, lpwfdData);
    return true;
  }

  SetError(ERROR_NO_MORE_FILES);
  return false;
}

void Plugin::FindClose(PluginFindData* pFindData) {
  delete pFindData;
}

// --- File Information & Attributes ---

LPVFSFILEDATAHEADER Plugin::GetfileInformation(Path path, HANDLE heap) {
  SetError(ERROR_FILE_NOT_FOUND);
  if (!ChangeDir(path.parent_path()))
    return nullptr;
  auto filename = path.filename().wstring();

  auto iter = mCurrentDir->children_.find(path.filename().wstring());
  if (iter == mCurrentDir->children_.end())
    return {};

  SetError(0);
  return GetVFSforEntry(iter->first, iter->second, heap);
}

bool Plugin::GetFileSize(Path path, PluginFile* file, uint64_t* piFileSize) {
  if (!ChangeDir(path))
    return false;

  if (!mCurrentDir->is_file_)
    return false;

  *piFileSize = mCurrentDir->entry_->unpack_size();
  return true;
}

bool Plugin::GetFileAttr(Path path, LPDWORD pAttr) {
  if (!ChangeDir(path))
    return false;

  if (mCurrentDir->is_file_) {
    *pAttr = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_COMPRESSED;
  } else {
    *pAttr = FILE_ATTRIBUTE_DIRECTORY;
  }
  return true;
}

// --- Extraction ---

bool Plugin::Extract(LPVOID func_data, Path source_path, Path target_path) {
  if (!ChangeDir(source_path.parent_path()))
    return false;

  auto iter = mCurrentDir->children_.find(source_path.filename().wstring());
  if (iter == mCurrentDir->children_.end())
    return false;

  if (iter->second.is_file_) {
    Path target = target_path;
    target.append(source_path.filename());
    return ExtractFile(func_data, iter->second, target);
  } else {
    Path target = target_path;
    target.append(source_path.filename());
    return ExtractPath(func_data, source_path, target);
  }
}

bool Plugin::ExtractFile(LPVOID func_data, const DirEnt& entry, Path target_path) {
  if (!entry.is_file_)
    return false;

  std::ofstream target(target_path.wstring().c_str(),
                       std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
  for (auto segment : entry.entry_->segments()) {
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
  DOpus.AddFunctionFileChange(func_data, /* fIsDest= */ false, OPUSFILECHANGE_CREATE, target_path.wstring().c_str());

  return true;
}

bool Plugin::ExtractPath(LPVOID func_data, Path source_path, Path target_path) {
  if (!ChangeDir(source_path))
    return false;

  std::vector<std::wstring> children;
  for (const auto& [name, entry] : mCurrentDir->children_) {
    children.push_back(name);
  }

  target_path.create_directories();

  bool success = true;
  for (const auto& child : children) {
    Path child_path = source_path;
    child_path.append(Path(child));
    if (!Extract(func_data, child_path, target_path)) {
      success = false;
    }
  }
  return success;
}

bool Plugin::ExtractEntries(LPVOID func_data, dopus::wstring_view_span entry_names, Path target_path) {
  SetError(0);
  for (auto name : entry_names) {
    Path source_path = Path(name);
    Extract(func_data, source_path, target_path);
  }

  return true;
}

// --- Plugin API Specifics ---

int Plugin::ContextVerb(LPVFSCONTEXTVERBDATAW lpVerbData) {
  Path full_path = Path(lpVerbData->lpszPath);
  if (!ChangeDir(full_path.parent_path()))
    return VFSCVRES_FAIL;

  auto item = mCurrentDir->children_.find(full_path.filename().wstring());

  if (item == mCurrentDir->children_.end())
    return VFSCVRES_FAIL;
  if (!item->second.is_file_)
    return VFSCVRES_DEFAULT;

  return VFSCVRES_EXTRACT;
}

uint32_t Plugin::BatchOperation(Path path, LPVFSBATCHDATAW lpBatchData) {
  switch (lpBatchData->uiOperation) {
    case VFSBATCHOP_EXTRACT:
      if (ExtractEntries(lpBatchData->lpFuncData, dopus::wstring_view_span(lpBatchData->pszFiles),
                         Path(lpBatchData->pszDestPath)))
        return VFSBATCHRES_HANDLED;
      break;

    default:
      break;
  }
  /* Not implemented */
  return VFSBATCHRES_ABORT;
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
      *reinterpret_cast<LPDWORD>(lpPropData) = true;
      break;

    case VFSPROP_SHOWFULLPROGRESSBAR:  // No progress bar even when copying.
      *reinterpret_cast<LPDWORD>(lpPropData) = false;
      break;

    case VFSPROP_DRAGEFFECTS:
      *reinterpret_cast<LPDWORD>(lpPropData) = DROPEFFECT_COPY;
      break;

    case VFSPROP_BATCHOPERATION:
      *reinterpret_cast<LPDWORD>(lpPropData) = VFSBATCHRES_HANDLED;
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

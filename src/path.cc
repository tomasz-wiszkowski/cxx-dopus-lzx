#include "path.hh"

#include <windows.h>

#include <pathcch.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <map>

void Path::normalize_separators() {
  while (!path_.empty() && path_.back() == L'\\') {
    path_.pop_back();
  }
}

Path Path::from_foreign_path(std::wstring foreign_path) {
  std::map<std::wstring_view, std::wstring_view> reserved_characters = {
      {L"\\", L"\uFF3C"},  // ＼
      {L"?", L"\uFF1F"},   // ？
      {L"*", L"\uFF0A"},   // ＊
      {L"|", L"\uFF5C"},   // ｜
      {L"\"", L"\uFF02"},  // ＂
      {L"<", L"\uFF1C"},   // ＜
      {L">", L"\uFF1E"},   // ＞
      {L":", L"\uFF1A"},   // ：
  };

  for (const auto& [from, to] : reserved_characters) {
    size_t pos = 0;
    while ((pos = foreign_path.find(from, pos)) != std::wstring::npos) {
      foreign_path.replace(pos, from.length(), to);
      pos += to.length();
    }
  }

  std::map<std::wstring_view, std::wstring_view> reserved_names = {
      {L".", L"\uFF0E"},
      {L"..", L"\uFF0E\uFF0E"},
      // Names known to be reserved in Windows, even with extensions. See
      // https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
      {L"CON", L"CON_"},
      {L"PRN", L"PRN_"},
      {L"AUX", L"AUX_"},
      {L"NUL", L"NUL_"},
      {L"COM1", L"COM1_"},
      {L"COM2", L"COM2_"},
      {L"COM3", L"COM3_"},
      {L"COM4", L"COM4_"},
      {L"COM5", L"COM5_"},
      {L"COM6", L"COM6_"},
      {L"COM7", L"COM7_"},
      {L"COM8", L"COM8_"},
      {L"COM9", L"COM9_"},
      {L"LPT1", L"LPT1_"},
      {L"LPT2", L"LPT2_"},
      {L"LPT3", L"LPT3_"},
      {L"LPT4", L"LPT4_"},
      {L"LPT5", L"LPT5_"},
      {L"LPT6", L"LPT6_"},
      {L"LPT7", L"LPT7_"},
      {L"LPT8", L"LPT8_"},
      {L"LPT9", L"LPT9_"},
      // Some more exotic ones
      {L"COM¹", L"COM¹_"},
      {L"COM²", L"COM²_"},
      {L"COM³", L"COM³_"},
      {L"LPT¹", L"LPT¹_"},
      {L"LPT²", L"LPT²_"},
      {L"LPT³", L"LPT³_"},
  };

  for (const auto& [from, to] : reserved_names) {
    size_t pos = 0;
    while ((pos = foreign_path.find(from, pos)) != std::wstring::npos) {
      // Only replace if
      // 1. It's at the start of the path or preceded by a separator.
      // 2. It's at the end of the path or followed by a separator.
      bool valid_before = (pos == 0) || (foreign_path[pos - 1] == L'/');
      bool valid_after = (pos + from.length() == foreign_path.length()) || (foreign_path[pos + from.length()] == L'/');

      if (valid_before && valid_after) {
        foreign_path.replace(pos, from.length(), to);
        pos += to.length();
      } else {
        pos += from.length();
      }
    }
  }

  // Replace all `/` with `\\` to avoid confusion.
  std::replace(foreign_path.begin(), foreign_path.end(), L'/', L'\\');

  return Path(std::move(foreign_path));
}

Path& Path::append(const Path& p) {
  if (p.empty())
    return *this;

  wchar_t buffer[PATHCCH_MAX_CCH];
  HRESULT hr = PathCchCombineEx(buffer, PATHCCH_MAX_CCH, path_.c_str(), p.path_.c_str(), PATHCCH_ALLOW_LONG_PATHS);
  if (SUCCEEDED(hr)) {
    path_ = buffer;
  }
  normalize_separators();
  return *this;
}

Path Path::parent_path() const {
  if (path_.empty())
    return Path();
  wchar_t buffer[PATHCCH_MAX_CCH];
  wcscpy_s(buffer, PATHCCH_MAX_CCH, path_.c_str());
  HRESULT hr = PathCchRemoveFileSpec(buffer, PATHCCH_MAX_CCH);
  if (SUCCEEDED(hr)) {
    return Path(buffer);
  }
  return Path();
}

Path Path::filename() const {
  PCWSTR pszFileName = PathFindFileNameW(path_.c_str());
  if (pszFileName) {
    return Path(pszFileName);
  }
  return *this;
}

Path Path::extension() const {
  const wchar_t* ext;
  HRESULT hr = PathCchFindExtension(path_.c_str(), path_.length() + 1, &ext);
  if (SUCCEEDED(hr) && ext && *ext) {
    return Path(ext);
  }
  return Path();
}

bool Path::exists() const {
  return GetFileAttributesW(path_.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::optional<Path> Path::relative_to(const Path& base) const {
  wchar_t buffer[MAX_PATH];
  if (!PathRelativePathToW(buffer, base.path_.c_str(), FILE_ATTRIBUTE_DIRECTORY, path_.c_str(),
                           FILE_ATTRIBUTE_DIRECTORY)) {
    return {};
  }

  // PathRelativePathToW will return ".." if it's not a subpath.
  if (wcsncmp(buffer, L"..\\", 3) == 0)
    return {};

  // It may also return ".\" for the path itself.
  if (wcsncmp(buffer, L".\\", 2) == 0)
    return Path(buffer + 2);

  return Path(buffer);
}

std::vector<std::wstring_view> Path::path_components() const {
  std::vector<std::wstring_view> components;
  size_t start = 0;
  while (start < path_.size()) {
    size_t end = path_.find(L'\\', start);
    if (end == std::wstring::npos) {
      components.emplace_back(std::wstring_view(path_).substr(start));
      break;
    } else {
      components.emplace_back(std::wstring_view(path_).substr(start, end - start));
      start = end + 1;
    }
  }
  return components;
}

void Path::create_directories() const {
  SHCreateDirectoryExW(nullptr, path_.c_str(), nullptr);
}

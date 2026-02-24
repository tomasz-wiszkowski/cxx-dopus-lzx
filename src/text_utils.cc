#include "text_utils.hh"

#include <windows.h>

std::wstring latin1_to_wstring(std::string_view latin1) {
  int wide_len = MultiByteToWideChar(28591,  // ISO-8859-1 code page
                                     /* flags= */ 0, latin1.data(), static_cast<int>(latin1.size()), nullptr, 0);

  std::wstring wide(wide_len, 0);

  MultiByteToWideChar(28591, 0, latin1.data(), static_cast<int>(latin1.size()), &wide[0], wide_len);

  // Truncate the null terminator added by MultiByteToWideChar
  if (!wide.empty() && wide.back() == L'\0') {
    wide.pop_back();
  }

  return wide;
}

std::wstring utf8_to_wstring(std::string_view utf8) {
    if (utf8.empty()) return {};

    int size = MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), utf8.size(),
        nullptr, 0);

    std::wstring result(size, 0);

    MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), utf8.size(),
        result.data(), size);

    return result;
}

std::string wstring_to_latin1(std::wstring_view wide) {
  int latin1_len =
      WideCharToMultiByte(28591,  // ISO-8859-1 code page
                          WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, "?", nullptr);

  std::string latin1(latin1_len, 0);

  WideCharToMultiByte(28591, WC_NO_BEST_FIT_CHARS, wide.data(), static_cast<int>(wide.size()), &latin1[0], latin1_len,
                      "?", nullptr);

  // Truncate the null terminator added by WideCharToMultiByte
  if (!latin1.empty() && latin1.back() == '\0') {
    latin1.pop_back();
  }

  return latin1;
}

std::filesystem::path sanitize(std::filesystem::path in) {
  // Remove trailing `/`
  if (!in.has_filename())
    in = in.parent_path();
  // Convert all the `.` and `..` to an actual target path.
  return in.lexically_normal();
}

bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& node) {
  auto b = base.lexically_normal();
  auto n = node.lexically_normal();

  return std::mismatch(b.begin(), b.end(), n.begin(), n.end()).first == b.end();
}

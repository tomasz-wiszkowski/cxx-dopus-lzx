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
  if (utf8.empty())
    return {};

  int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(), nullptr, 0);

  std::wstring result(size, 0);

  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(), result.data(), size);

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

std::string wstring_to_utf8(std::wstring_view wide) {
  if (wide.empty())
    return {};

  int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide.size(), nullptr, 0, nullptr, nullptr);

  std::string result(size, 0);

  WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide.size(), result.data(), size, nullptr, nullptr);

  return result;
}

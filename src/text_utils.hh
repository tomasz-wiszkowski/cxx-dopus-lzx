#pragma once

#include <string>
#include <string_view>

/// @brief Convert a latin1 string to wstring
/// @param latin1 Input string in latin1 encoding
/// @return widestring in UTF-16 encoding
std::wstring latin1_to_wstring(std::string_view latin1);

/// @brief Convert an utf8 string to wstring
/// @param utf8 Input string in utf8 encoding
/// @return widestring in utf-8 encoding.
std::wstring utf8_to_wstring(std::string_view utf8);

/// @brief Convert a wide string to latin1 encoding
/// @param wide Input string in wide character encoding (UTF-16 on Windows)
/// @return latin1 encoded string
std::string wstring_to_latin1(std::wstring_view wide);

/// @brief Convert a wide string to UTF-8 encoding
/// @param wide Input string in wide character encoding (UTF-16 on Windows)
/// @return UTF-8 encoded string
std::string wstring_to_utf8(std::wstring_view wide);

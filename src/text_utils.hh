#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// @brief Convert a latin1 string to UTF-8
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

/// @brief Clean up input paths ensuring that it always contains the "filename" stem, even if pointing to a directory.
/// @param in The path to sanitize.
/// @return sanitized path.
std::filesystem::path sanitize(std::filesystem::path in);

/// @brief Returns whether `node` path points to a root or a sub-element of the `base` path.
/// @param base The base path (e.g. namespace).
/// @param node The node path to check.
bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& node);

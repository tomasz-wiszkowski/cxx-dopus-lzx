#pragma once

#include <compare>
#include <optional>
#include <string>
#include <vector>

class Path {
 public:
  Path() = default;
  Path(const Path&) = default;
  Path(Path&&) = default;
  Path& operator=(const Path&) = default;
  Path& operator=(Path&&) = default;

  // Constructors
  explicit Path(std::wstring_view p) : path_(p) { normalize_separators(); }
  static Path from_foreign_path(std::wstring foreign_path);

  // Modifiers
  Path& append(const Path& p);
  void clear() { path_.clear(); }
  bool empty() const { return path_.empty(); }

  // Accessors
  const std::wstring& wstring() const { return path_; }

  // Manipulation
  Path parent_path() const;
  Path filename() const;
  Path extension() const;
  std::optional<Path> relative_to(const Path& base) const;
  std::vector<std::wstring_view> path_components() const;

  // Filesystem operations
  bool exists() const;
  void create_directories() const;

  // Comparison
  auto operator<=>(const Path& other) const { return path_ <=> other.path_; }
  auto operator<=>(std::wstring_view other) const { return path_ <=> other; }

 private:
  void normalize_separators();

  std::wstring path_;
};

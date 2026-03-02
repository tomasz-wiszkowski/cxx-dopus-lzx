#include "path.hh"

#include <gtest/gtest.h>

TEST(PathTest, SanitizeForeignPath) {
  // No backslashes in names.
  EXPECT_EQ(Path::from_foreign_path(L"bar\\baz").wstring(), L"bar\uFF3Cbaz");

  // Substitution test -- No dot entries (one variant _should_ suffice)
  EXPECT_EQ(Path::from_foreign_path(L".").wstring(), L"\uFF0E");
  EXPECT_EQ(Path::from_foreign_path(L"./foo").wstring(), L"\uFF0E\\foo");
  EXPECT_EQ(Path::from_foreign_path(L"foo/.").wstring(), L"foo\\\uFF0E");
  EXPECT_EQ(Path::from_foreign_path(L"foo/./bar").wstring(), L"foo\\\uFF0E\\bar");

  // But don't touch the dots in the middle of names.
  EXPECT_EQ(Path::from_foreign_path(L".foo.").wstring(), L".foo.");
  EXPECT_EQ(Path::from_foreign_path(L"foo./bar").wstring(), L"foo.\\bar");
  EXPECT_EQ(Path::from_foreign_path(L"foo/.bar").wstring(), L"foo\\.bar");
  EXPECT_EQ(Path::from_foreign_path(L".foo/bar.").wstring(), L".foo\\bar.");

  // Already covered above but for completeness...
  EXPECT_EQ(Path::from_foreign_path(L"foo/bar").wstring(), L"foo\\bar");
}

TEST(PathTest, ParentPath) {
  EXPECT_EQ(Path(L"C:\\foo\\bar.txt").parent_path().wstring(), L"C:\\foo");
  EXPECT_EQ(Path(L"C:\\foo\\bar\\").parent_path().wstring(), L"C:\\foo");
  EXPECT_EQ(Path(L"C:\\").parent_path().wstring(), L"C:");
  EXPECT_EQ(Path(L"foo.txt").parent_path().wstring(), L"");
  EXPECT_EQ(Path(L"").parent_path().wstring(), L"");
}

TEST(PathTest, Filename) {
  EXPECT_EQ(Path(L"C:\\foo\\bar.txt").filename().wstring(), L"bar.txt");
  EXPECT_EQ(Path(L"foo.txt").filename().wstring(), L"foo.txt");

  // These are not technically filenames, but for a lack of better name, let's stick with these.
  EXPECT_EQ(Path(L"C:\\foo\\bar\\").filename().wstring(), L"bar");
  EXPECT_EQ(Path(L"C:\\").filename().wstring(), L"C:");
  EXPECT_EQ(Path(L"").filename().wstring(), L"");
}

TEST(PathTest, Extension) {
  EXPECT_EQ(Path(L"foo.txt").extension().wstring(), L".txt");
  EXPECT_EQ(Path(L"foo.tar.gz").extension().wstring(), L".gz");
  EXPECT_EQ(Path(L"foo.").extension().wstring(), L".");
  EXPECT_EQ(Path(L"foo").extension().wstring(), L"");
  EXPECT_EQ(Path(L".foo").extension().wstring(), L".foo");
  EXPECT_EQ(Path(L"").extension().wstring(), L"");
}

TEST(PathTest, PathComponents) {
  EXPECT_EQ(Path(L"").path_components().size(), 0);
  EXPECT_EQ(Path(L"foo").path_components(), (std::vector<std::wstring_view>{L"foo"}));
  EXPECT_EQ(Path(L"foo\\bar\\baz").path_components(), (std::vector<std::wstring_view>{L"foo", L"bar", L"baz"}));
  EXPECT_EQ(Path(L"C:\\foo\\bar").path_components(), (std::vector<std::wstring_view>{L"C:", L"foo", L"bar"}));
  EXPECT_EQ(Path(L"C:\\").path_components(), (std::vector<std::wstring_view>{L"C:"}));
  EXPECT_EQ(Path(L"foo\\bar\\").path_components(), (std::vector<std::wstring_view>{L"foo", L"bar"}));
}

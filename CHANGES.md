# Changelog

## v0.4

- Removed `std::filesystem` dependency due to limited control over thrown 
  exceptions.

## v0.3

- Support empty directory entries
- Add support for comments and basic support for file protection bits
- Add basic support for date stamp decoding (best-effort).

LZX date encoding is unnecessarily complicated and unfortunately further
fragmented by different implementations. Despite date stamp being a piece
of file metadata, it's appears less critical than names, comments, contents.

This version intentionally avoids implementing workarounds for broad set of
bugs and versions of date stamp encoding - the complexity behind these is 
unwarranted and not worth the effort at this point.

## v0.2

- Address issues with `std::filesystem` throwing for unclear reasons when
  requesting relative path component.

## v0.1

Initial release with basic support for lzx archive files.
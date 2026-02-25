#pragma once

#include <filesystem>
#include <optional>
#include <set>

#include "dopus_wstring_view_span.hh"
#include "unlzx.hh"

/// @brief Guard object to set and restore fields.
template <typename T>
class Guard {
 private:
  T& old_;
  T& new_;

 public:
  /// @brief Constructs a Guard object, swapping the values of hold and hnew.
  /// @param hold Reference to the old value.
  /// @param hnew Reference to the new value to swap in.
  Guard(T& hold, T& hnew) : old_(hold), new_(hnew) { std::swap(old_, new_); }

  /// @brief Destructor that restores the original value.
  ~Guard() { std::swap(old_, new_); }

  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
  Guard(Guard&&) = delete;
  Guard& operator=(Guard&&) = delete;
};

/// @brief Opaque handle for file enumeration.
struct PluginFindData;

/// @brief Represents an open file within the archive.
struct PluginFile {
  LzxEntry* file_{};
  size_t offset_{};
};

/// @brief Main plugin class handling LZX archive interactions.
class Plugin {
 public:
  struct DirEnt {
    DirEnt() = default;
    DirEnt(const DirEnt&) = delete;
    DirEnt& operator=(const DirEnt&) = delete;

    std::map<std::string, DirEnt> children_;
    LzxEntry* file_{};
  };

 private:

  using EntryType = void*;
  HANDLE mAbortEvent{};
  std::filesystem::path mPath;
  std::shared_ptr<Unlzx> mArchive;
  std::shared_ptr<std::map<std::string, LzxEntry>> mFlatMap;
  std::shared_ptr<DirEnt> mRoot;
  DirEnt* mCurrentDir;
  int mLastError{};

  // --- Directory Structure & Navigation ---

  /// @brief Re-reads the directory structure from the current archive, reconstructing mFlatMap and mRoot.
  void ReconstructDirStructure();

  /// @brief Navigate to a specific (absolute) path within the archive.
  /// @param dir The absolute path to navigate to.
  /// @return true if successful, false otherwise.
  bool ChangeDir(std::filesystem::path dir);

  // --- Entry Information ---

  /// @brief Retrieves VFS file data header for a given directory entry.
  /// @param name The name of the entry.
  /// @param entry The directory entry.
  /// @param heap Handle to the heap for memory allocation.
  /// @return Pointer to the allocated VFSFILEDATAHEADER.
  LPVFSFILEDATAHEADER GetVFSforEntry(const std::string& name, const DirEnt& entry, HANDLE heap);

  /// @brief Populates WIN32_FIND_DATAW for a given directory entry.
  /// @param name The name of the entry.
  /// @param entry The directory entry.
  /// @param data Pointer to the WIN32_FIND_DATAW structure to populate.
  void GetWfdForEntry(const std::string& name, const DirEnt& entry, LPWIN32_FIND_DATAW data);

  /// @brief Retrieves the file time for a given entry.
  /// @param entry The entry to retrieve the time for.
  /// @return The file time.
  FILETIME GetFileTime(const EntryType& entry);

  // --- State Management & Helpers ---

  /// @brief Sets the abort event handle and returns a guard to restore it.
  /// @param hAbortEvent The new abort event handle.
  /// @return A Guard object managing the handle restoration.
  Guard<HANDLE> SetAbortHandle(HANDLE& hAbortEvent);

  /// @brief Checks if an abort has been requested.
  /// @return true if abort was requested, false otherwise.
  bool ShouldAbort() const;

  /// @brief Sets the last error code.
  /// @param error The error code to set.
  void SetError(int error);

 public:
  // --- Initialization & Archive Info ---

  /// @brief Loads an LZX archive from the specified path.
  /// @param pAfPath Path to the archive file.
  /// @return Optional path to the loaded archive if successful.
  std::optional<std::filesystem::path> LoadFile(std::filesystem::path path);

  /// @brief Returns the available size in the archive.
  /// @return The available size in bytes.
  size_t GetAvailableSize();

  /// @brief Returns the total size of the archive.
  /// @return The total size in bytes.
  size_t GetTotalSize();

  /// @brief Returns the last error that occurred.
  /// @return The last error code.
  int GetError() const { return mLastError; }

  // --- Directory Reading ---

  /// @brief Reads the contents of the current directory into the VFS.
  /// @param lpRDD Pointer to the VFSREADDIRDATAW structure.
  /// @return true if successful, false otherwise.
  bool ReadDirectory(LPVFSREADDIRDATAW lpRDD);

  // --- File I/O ---

  /// @brief Opens a file within the archive.
  /// @param path The path of the file to open.
  /// @param for_writing true if opening for writing, false for reading.
  /// @return Pointer to the opened PluginFile.
  PluginFile* OpenFile(std::filesystem::path path, bool for_writing);

  /// @brief Reads data from an open file.
  /// @param pFile Pointer to the open file.
  /// @param buffer Buffer to read data into.
  /// @param readSize Pointer to a DWORD to receive the number of bytes read.
  /// @return true if successful, false otherwise.
  bool ReadFile(PluginFile* pFile, std::span<uint8_t> buffer, LPDWORD readSize);

  /// @brief Closes an open file.
  /// @param pFile Pointer to the file to close.
  void CloseFile(PluginFile* pFile);

  // --- File Enumeration ---

  /// @brief Begins a file enumeration in the specified path.
  /// @param path The path to enumerate.
  /// @param lpwfdData Pointer to WIN32_FIND_DATA structure to receive the first file's data.
  /// @param hAbortEvent Handle to an abort event.
  /// @return Pointer to a PluginFindData handle.
  PluginFindData* FindFirst(std::filesystem::path path, LPWIN32_FIND_DATA lpwfdData, HANDLE hAbortEvent);

  /// @brief Continues a file enumeration.
  /// @param lpRAF Pointer to the PluginFindData handle.
  /// @param lpwfdData Pointer to WIN32_FIND_DATA structure to receive the next file's data.
  /// @return true if successful, false otherwise.
  bool FindNext(PluginFindData* lpRAF, LPWIN32_FIND_DATA lpwfdData);

  /// @brief Closes a file enumeration handle.
  /// @param lpRAF Pointer to the PluginFindData handle.
  void FindClose(PluginFindData* lpRAF);

  // --- File Information & Attributes ---

  /// @brief Retrieves information for a file at the given path.
  /// @param path The path to the file.
  /// @param heap Handle to the heap for memory allocation.
  /// @return Pointer to the allocated VFSFILEDATAHEADER.
  LPVFSFILEDATAHEADER GetfileInformation(std::filesystem::path path, HANDLE heap);

  /// @brief Gets the size of a file.
  /// @param path The path to the file.
  /// @param file Optional pointer to an already opened PluginFile.
  /// @param piFileSize Pointer to receive the file size.
  /// @return true if successful, false otherwise.
  bool GetFileSize(std::filesystem::path path, PluginFile* file, uint64_t* piFileSize);

  /// @brief Gets the attributes of a file.
  /// @param path The path to the file.
  /// @param pAttr Pointer to receive the attributes.
  /// @return true if successful, false otherwise.
  bool GetFileAttr(std::filesystem::path path, LPDWORD pAttr);

  // --- Extraction ---

  /// @brief Extracts a file or folder from the archive.
  /// @param func_data Plugin-specific function data.
  /// @param source_path Path within the archive to extract.
  /// @param target_path Destination path on disk.
  /// @return true if successful, false otherwise.
  bool Extract(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path);

  /// @brief Extracts a specific file entry.
  /// @param func_data Plugin-specific function data.
  /// @param pEntry The entry to extract.
  /// @param target_path Destination path on disk.
  /// @return true if successful, false otherwise.
  bool ExtractFile(LPVOID func_data, const DirEnt& pEntry, std::filesystem::path target_path);

  /// @brief Extracts all files in a path.
  /// @param func_data Plugin-specific function data.
  /// @param source_path Directory path within the archive to extract.
  /// @param target_path Destination path on disk.
  /// @return true if successful, false otherwise.
  bool ExtractPath(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path);

  /// @brief Extracts multiple specific entries.
  /// @param func_data Plugin-specific function data.
  /// @param entry_names Span of entry names to extract.
  /// @param target_path Destination directory path on disk.
  /// @return true if successful, false otherwise.
  bool ExtractEntries(LPVOID func_data, dopus::wstring_view_span entry_names, std::filesystem::path target_path);

  // --- Plugin API Specifics ---

  /// @brief Executes a context menu verb.
  /// @param lpVerbData Pointer to the VFSCONTEXTVERBDATAW structure.
  /// @return Status code of the operation.
  int ContextVerb(LPVFSCONTEXTVERBDATAW lpVerbData);

  /// @brief Performs a batch operation.
  /// @param lpszPath The path for the operation.
  /// @param lpBatchData Pointer to the VFSBATCHDATAW structure.
  /// @return Status code of the batch operation.
  uint32_t BatchOperation(std::filesystem::path lpszPath, LPVFSBATCHDATAW lpBatchData);

  /// @brief Retrieves a plugin property.
  /// @param propId The ID of the property to retrieve.
  /// @param lpPropData Pointer to the property data.
  /// @param lpData1 Pointer to additional data.
  /// @param lpData2 Pointer to additional data.
  /// @param lpData3 Pointer to additional data.
  /// @return true if successful, false otherwise.
  bool PropGet(vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3);
};

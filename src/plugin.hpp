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
  Guard(T& hold, T& hnew) : old_(hold), new_(hnew) { std::swap(old_, new_); }

  ~Guard() { std::swap(old_, new_); }

  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
  Guard(Guard&&) = delete;
  Guard& operator=(Guard&&) = delete;
};

class PluginFindData {};

struct PluginFile {
  LzxEntry* file_{};
  size_t offset_{};
};

class Plugin {
 private:
  struct DirEnt {
    DirEnt() = default;
    DirEnt(const DirEnt&) = delete;
    DirEnt& operator=(const DirEnt&) = delete;

    std::map<std::string, DirEnt> children_;
    LzxEntry* file_{};
    std::filesystem::path extracted_path_;
  };

  using EntryType = void*;

  HANDLE mAbortEvent{};
  std::filesystem::path mPath;
  std::shared_ptr<Unlzx> mArchive;
  std::shared_ptr<std::map<std::string, LzxEntry>> mFlatMap;
  std::shared_ptr<DirEnt> mRoot;
  DirEnt* mCurrentDir;
  int mLastError{};

  /// @brief Re-reads the directory structure from the current archive, reconstructing mFlatMap and mRoot.
  void ReconstructDirStructure();

  /// @brief Navigate to a specific (absolute) path within the archive.
  bool ChangeDir(std::filesystem::path dir);

  LPVFSFILEDATAHEADER GetVFSforEntry(const std::string& name, const DirEnt& entry, HANDLE heap);
  void GetWfdForEntry(const std::string& name, const DirEnt& entry, LPWIN32_FIND_DATAW data);

  Guard<HANDLE> SetAbortHandle(HANDLE& hAbortEvent);
  bool ShouldAbort() const;

  FILETIME GetFileTime(const EntryType& entry);

  void SetError(int error);

 public:
  std::optional<std::filesystem::path> LoadFile(std::filesystem::path pAfPath);

  bool ReadDirectory(LPVFSREADDIRDATAW lpRDD);

  bool ReadFile(PluginFile* pFile, std::span<uint8_t> buffer, LPDWORD readSize);
  bool WriteFile(PluginFile* pFile, std::span<uint8_t> buffer, LPDWORD writeSize);
  PluginFile* OpenFile(std::filesystem::path path, bool for_writing);
  void CloseFile(PluginFile* pFile);

  size_t GetAvailableSize();
  size_t GetTotalSize();

  PluginFindData* FindFirst(std::filesystem::path path, LPWIN32_FIND_DATA lpwfdData, HANDLE hAbortEvent);
  bool FindNext(PluginFindData* lpRAF, LPWIN32_FIND_DATA lpwfdData);
  void FindClose(PluginFindData* lpRAF);

  LPVFSFILEDATAHEADER GetfileInformation(std::filesystem::path path, HANDLE heap);
  bool GetFileSize(std::filesystem::path path, PluginFile* file, uint64_t* piFileSize);
  bool GetFileAttr(std::filesystem::path path, LPDWORD pAttr);

  bool Extract(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path);
  bool ExtractFile(LPVOID func_data, const EntryType& pEntry, std::filesystem::path target_path);
  bool ExtractPath(LPVOID func_data, std::filesystem::path source_path, std::filesystem::path target_path);
  bool ExtractEntries(LPVOID func_data, dopus::wstring_view_span entry_names, std::filesystem::path target_path);

  int ContextVerb(LPVFSCONTEXTVERBDATAW lpVerbData);
  uint32_t BatchOperation(std::filesystem::path lpszPath, LPVFSBATCHDATAW lpBatchData);
  bool PropGet(vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3);
  int GetError() const { return mLastError; }
};

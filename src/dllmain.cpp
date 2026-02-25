#include <strsafe.h>

#include "dopus_wstring_view_span.hh"
#include "stdafx.h"

static constexpr GUID PluginGUID{0x4bcae8da, 0xd598, 0x4a67, {0xa0, 0x45, 0xbb, 0xbb, 0xb8, 0xaf, 0x58, 0xb0}};
HINSTANCE g_module_instance{};

extern "C" {
__declspec(dllexport) bool WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {
  g_module_instance = hInstance;
  return true;
}

__declspec(dllexport) bool VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
  lpVFSInfo->idPlugin = PluginGUID;
  lpVFSInfo->dwFlags = VFSF_CANSHOWABOUT;
  lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE;

  StringCchCopyW(lpVFSInfo->lpszHandleExts, lpVFSInfo->cchHandleExtsMax, L".lzx");
  StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"LZX");
  StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, L"LZX file support (read-only) v0.1");
  StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) Copyright 2026 Tomasz Wiszkowski");
  StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"github.com/tomasz-wiszkowski/cxx-dopus-lzx");

  return true;
}

__declspec(dllexport) bool VFS_ReadDirectoryW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPVFSREADDIRDATAW lpRDD) {
  return plugin->ReadDirectory(lpRDD);
}

__declspec(dllexport) Plugin* WINAPI VFS_Create(LPGUID pGuid) {
  return new Plugin();
}

__declspec(dllexport) Plugin* WINAPI VFS_Clone(Plugin* plugin) {
  return new Plugin(*plugin);
}

__declspec(dllexport) void WINAPI VFS_Destroy(Plugin* plugin) {
  delete plugin;
}

__declspec(dllexport) BOOL WINAPI VFS_CreateDirectoryW(Plugin* plugin,
                                                       LPVFSFUNCDATA lpFuncData,
                                                       LPTSTR lpszPath,
                                                       DWORD dwFlags) {
  return false;
}

__declspec(dllexport) PluginFile* WINAPI VFS_CreateFileW(Plugin* plugin,
                                                         LPVFSFUNCDATA lpVFSData,
                                                         LPWSTR lpszPath,
                                                         DWORD dwMode,
                                                         DWORD dwFileAttr,
                                                         DWORD dwFlags,
                                                         LPFILETIME lpFT) {
  return plugin->OpenFile(lpszPath, dwMode == GENERIC_WRITE);
}

__declspec(dllexport) bool WINAPI VFS_ReadFile(Plugin* plugin,
                                               LPVFSFUNCDATA lpVFSData,
                                               PluginFile* file,
                                               LPVOID lpData,
                                               DWORD dwSize,
                                               LPDWORD lpdwReadSize) {
  return plugin->ReadFile(file, std::span<uint8_t>(static_cast<uint8_t*>(lpData), dwSize), lpdwReadSize);
}

__declspec(dllexport) BOOL WINAPI VFS_WriteFile(Plugin* data,
                                                LPVFSFUNCDATA lpFuncData,
                                                PluginFile* file,
                                                LPVOID lpData,
                                                DWORD dwSize,
                                                BOOL fFlush,
                                                LPDWORD lpdwWriteSize) {
  return false;
}

__declspec(dllexport) BOOL WINAPI VFS_GetFileAttrW(Plugin* plugin,
                                                   LPVFSFUNCDATA lpFuncData,
                                                   LPTSTR lpszPath,
                                                   LPDWORD lpdwAttr) {
  return plugin->GetFileAttr(lpszPath, lpdwAttr);
}

__declspec(dllexport) BOOL WINAPI
VFS_GetFileCommentW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, LPTSTR lpszComment, int cchCommentMax) {
  ::memset(lpszComment, 0, cchCommentMax * sizeof(TCHAR));
  return true;
}

__declspec(dllexport) BOOL WINAPI VFS_GetFileDescriptionW(HANDLE hVFSData,
                                                          LPVFSFUNCDATA lpFuncData,
                                                          LPTSTR lpszPath,
                                                          LPTSTR lpszDescription,
                                                          int cchDescriptionMax) {
  ::memset(lpszDescription, 0, cchDescriptionMax * sizeof(TCHAR));
  return true;
}

__declspec(dllexport) BOOL WINAPI VFS_GetFileIconW(HANDLE hVFSData,
                                                   LPVFSFUNCDATA lpFuncData,
                                                   LPTSTR lpszFile,
                                                   LPINT lpiSysIconIndex,
                                                   HICON* phLargeIcon,
                                                   HICON* phSmallIcon,
                                                   LPBOOL lpfDestroyIcons,
                                                   LPTSTR lpszCacheName,
                                                   int cchCacheNameMax,
                                                   LPINT lpiCacheIndex) {
  return false;
}

__declspec(dllexport) BOOL WINAPI VFS_GetFileSizeW(Plugin* plugin,
                                                   LPVFSFUNCDATA lpFuncData,
                                                   LPTSTR lpszPath,
                                                   PluginFile* file,
                                                   unsigned __int64* piFileSize) {
  if (file != nullptr) {
    *piFileSize = file->file_ ? file->file_->unpack_size() : 0;
    return true;
  }
  return plugin->GetFileSize(lpszPath, file, piFileSize);
}

__declspec(dllexport) BOOL WINAPI
VFS_SetFileAttrW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, DWORD dwAttr, BOOL fForDelete) {
  return false;
}
__declspec(dllexport) BOOL WINAPI VFS_SetFileCommentW(Plugin* plugin,
                                                      LPVFSFUNCDATA lpFuncData,
                                                      LPTSTR lpszPath,
                                                      LPTSTR lpszComment) {
  return false;
}
__declspec(dllexport) BOOL WINAPI VFS_SetFileTimeW(Plugin* plugin,
                                                   LPVFSFUNCDATA lpFuncData,
                                                   LPTSTR lpszPath,
                                                   LPFILETIME lpCreateTime,
                                                   LPFILETIME lpAccessTime,
                                                   LPFILETIME lpWriteTime) {
  return false;
}

__declspec(dllexport) void WINAPI VFS_CloseFile(Plugin* plugin, LPVFSFUNCDATA lpVFSData, PluginFile* file) {
  plugin->CloseFile(file);
}

__declspec(dllexport) BOOL WINAPI VFS_MoveFileW(Plugin* hVFSData,
                                                LPVFSFUNCDATA lpFuncData,
                                                LPTSTR lpszOldPath,
                                                LPTSTR lpszNewPath) {
  return false;
}

__declspec(dllexport) int VFS_ContextVerbW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPVFSCONTEXTVERBDATAW lpVerbData) {
  return plugin->ContextVerb(lpVerbData);
}

__declspec(dllexport) UINT WINAPI VFS_BatchOperationW(Plugin* plugin,
                                                      LPVFSFUNCDATA lpVFSData,
                                                      LPWSTR lpszPath,
                                                      LPVFSBATCHDATAW lpBatchData) {
  return plugin->BatchOperation(lpszPath, lpBatchData);
}

__declspec(dllexport) bool VFS_PropGetW(Plugin* plugin,
                                        vfsProperty propId,
                                        LPVOID lpPropData,
                                        LPVOID lpData1,
                                        LPVOID lpData2,
                                        LPVOID lpData3) {
  return plugin->PropGet(propId, lpPropData, lpData1, lpData2, lpData3);
}

__declspec(dllexport) long VFS_GetLastError(Plugin* data) {
  return data->GetError();
}

__declspec(dllexport) bool VFS_GetFreeDiskSpaceW(Plugin* plugin,
                                                 LPVFSFUNCDATA lpFuncData,
                                                 LPWSTR lpszPath,
                                                 unsigned __int64* piFreeBytesAvailable,
                                                 unsigned __int64* piTotalBytes,
                                                 unsigned __int64* piTotalFreeBytes) {
  if (!plugin->LoadFile(lpszPath))
    return false;
  if (piFreeBytesAvailable)
    *piFreeBytesAvailable = plugin->GetAvailableSize();
  if (piTotalFreeBytes)
    *piTotalFreeBytes = plugin->GetAvailableSize();
  if (piTotalBytes)
    *piTotalBytes = plugin->GetTotalSize();

  return true;
}

__declspec(dllexport) PluginFindData* WINAPI VFS_FindFirstFileW(Plugin* plugin,
                                                                LPVFSFUNCDATA lpVFSData,
                                                                LPWSTR lpszPath,
                                                                LPWIN32_FIND_DATA lpwfdData,
                                                                HANDLE hAbortEvent) {
  return plugin->FindFirst(lpszPath, lpwfdData, hAbortEvent);
}

__declspec(dllexport) BOOL WINAPI VFS_FindNextFileW(Plugin* plugin,
                                                    LPVFSFUNCDATA lpVFSData,
                                                    PluginFindData* find_data,
                                                    LPWIN32_FIND_DATA lpwfdData) {
  return plugin->FindNext(find_data, lpwfdData);
}

__declspec(dllexport) void WINAPI VFS_FindClose(Plugin* plugin, PluginFindData* find_data) {
  plugin->FindClose(find_data);
}

__declspec(dllexport) BOOL WINAPI VFS_ExtractFilesW(Plugin* plugin,
                                                    LPVFSFUNCDATA lpFuncData,
                                                    LPVFSEXTRACTFILESDATAW lpExtractData) {
  return plugin->ExtractEntries(lpFuncData, dopus::wstring_view_span(lpExtractData->lpszFiles),
                                lpExtractData->lpszDestPath);
}

__declspec(dllexport) bool VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
  return true;
}

__declspec(dllexport) bool VFS_Init(LPVFSINITDATA pInitData) {
  return true;
}

__declspec(dllexport) void VFS_Uninit() {}

__declspec(dllexport) LPVFSFILEDATAHEADER WINAPI
VFS_GetFileInformationW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
  return plugin->GetfileInformation(lpszPath, hHeap);
}

}  // extern "C"

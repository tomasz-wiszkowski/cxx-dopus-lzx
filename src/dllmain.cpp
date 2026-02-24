#include <strsafe.h>

#include "dopus_wstring_view_span.hh"
#include "stdafx.h"

static constexpr GUID PluginGUID{0x4bcae8da, 0xd598, 0x4a67, {0xa0, 0x45, 0xbb, 0xbb, 0xb8, 0xaf, 0x58, 0xb0}};
HINSTANCE g_module_instance{};

extern "C" {
__declspec(dllexport) bool VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo);
__declspec(dllexport) bool VFS_ReadDirectoryW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPVFSREADDIRDATAW lpRDD);

__declspec(dllexport) Plugin* WINAPI VFS_Create(LPGUID pGuid);
__declspec(dllexport) Plugin* WINAPI VFS_Clone(Plugin* plugin);
__declspec(dllexport) void WINAPI VFS_Destroy(Plugin* plugin);

__declspec(dllexport) BOOL WINAPI VFS_CreateDirectoryW(Plugin* plugin,
                                                       LPVFSFUNCDATA lpFuncData,
                                                       LPTSTR lpszPath,
                                                       DWORD dwFlags);

__declspec(dllexport) PluginFile* WINAPI VFS_CreateFileW(Plugin* plugin,
                                                         LPVFSFUNCDATA lpVFSData,
                                                         LPWSTR lpszPath,
                                                         DWORD dwMode,
                                                         DWORD dwFileAttr,
                                                         DWORD dwFlags,
                                                         LPFILETIME lpFT);

__declspec(dllexport) bool WINAPI VFS_ReadFile(Plugin* plugin,
                                               LPVFSFUNCDATA lpVFSData,
                                               PluginFile* file,
                                               LPVOID lpData,
                                               DWORD dwSize,
                                               LPDWORD lpdwReadSize);

__declspec(dllexport) BOOL WINAPI VFS_WriteFile(Plugin* data,
                                                LPVFSFUNCDATA lpFuncData,
                                                PluginFile* file,
                                                LPVOID lpData,
                                                DWORD dwSize,
                                                BOOL fFlush,
                                                LPDWORD lpdwWriteSize);

__declspec(dllexport) BOOL WINAPI
VFS_SetFileAttrW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, DWORD dwAttr, BOOL fForDelete);
__declspec(dllexport) BOOL WINAPI VFS_SetFileCommentW(Plugin* plugin,
                                                      LPVFSFUNCDATA lpFuncData,
                                                      LPTSTR lpszPath,
                                                      LPTSTR lpszComment);
__declspec(dllexport) BOOL WINAPI VFS_SetFileTimeW(Plugin* plugin,
                                                   LPVFSFUNCDATA lpFuncData,
                                                   LPTSTR lpszPath,
                                                   LPFILETIME lpCreateTime,
                                                   LPFILETIME lpAccessTime,
                                                   LPFILETIME lpWriteTime);

__declspec(dllexport) void WINAPI VFS_CloseFile(Plugin* plugin, LPVFSFUNCDATA lpVFSData, PluginFile* file);

__declspec(dllexport) BOOL WINAPI VFS_MoveFileW(Plugin* hVFSData,
                                                LPVFSFUNCDATA lpFuncData,
                                                LPTSTR lpszOldPath,
                                                LPTSTR lpszNewPath);

__declspec(dllexport) int VFS_ContextVerbW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPVFSCONTEXTVERBDATAW lpVerbData);
__declspec(dllexport) UINT WINAPI VFS_BatchOperationW(Plugin* plugin,
                                                      LPVFSFUNCDATA lpVFSData,
                                                      LPWSTR lpszPath,
                                                      LPVFSBATCHDATAW lpBatchData);
__declspec(dllexport) bool VFS_PropGetW(Plugin* hVFSData,
                                        vfsProperty propId,
                                        LPVOID lpPropData,
                                        LPVOID lpData1,
                                        LPVOID lpData2,
                                        LPVOID lpData3);
__declspec(dllexport) long VFS_GetLastError(Plugin* data);

__declspec(dllexport) bool VFS_GetFreeDiskSpaceW(Plugin* plugin,
                                                 LPVFSFUNCDATA lpFuncData,
                                                 LPWSTR lpszPath,
                                                 unsigned __int64* piFreeBytesAvailable,
                                                 unsigned __int64* piTotalBytes,
                                                 unsigned __int64* piTotalFreeBytes);

__declspec(dllexport) PluginFindData* WINAPI VFS_FindFirstFileW(Plugin* plugin,
                                                                LPVFSFUNCDATA lpVFSData,
                                                                LPWSTR lpszPath,
                                                                LPWIN32_FIND_DATA lpwfdData,
                                                                HANDLE hAbortEvent);
__declspec(dllexport) BOOL WINAPI VFS_FindNextFileW(Plugin* plugin,
                                                    LPVFSFUNCDATA lpVFSData,
                                                    PluginFindData* find_data,
                                                    LPWIN32_FIND_DATA lpwfdData);
__declspec(dllexport) void WINAPI VFS_FindClose(Plugin* plugin, PluginFindData find_data);

__declspec(dllexport) BOOL WINAPI VFS_ExtractFilesW(Plugin* plugin,
                                                    LPVFSFUNCDATA lpFuncData,
                                                    LPVFSEXTRACTFILESDATAW lpExtractData);

__declspec(dllexport) bool VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
__declspec(dllexport) bool VFS_Init(LPVFSINITDATA pInitData);
__declspec(dllexport) void VFS_Uninit();
__declspec(dllexport) LPVFSFILEDATAHEADER WINAPI
VFS_GetFileInformationW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags);
};

extern "C" int WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {
  g_module_instance = hInstance;
  return 1;
}

// Initialise plugin
bool VFS_Init(LPVFSINITDATA pInitData) {
  return true;
}

void VFS_Uninit() {}

bool VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
  lpVFSInfo->idPlugin = PluginGUID;
  lpVFSInfo->dwFlags = VFSF_CANSHOWABOUT;
  lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE;

  StringCchCopyW(lpVFSInfo->lpszHandleExts, lpVFSInfo->cchHandleExtsMax, L".lzx");
  StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"LZX");
  StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, L"LZX file support (read-only)");
  StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) Copyright 2026 Tomasz Wiszkowski");
  StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"github.com/tomasz-wiszkowski/cxx-dopus-lzx");

  return true;
}

bool VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
  return true;
}

Plugin* VFS_Create(LPGUID pGuid) {
  return new Plugin();
}

Plugin* VFS_Clone(Plugin* plugin) {
  return new Plugin(*plugin);
}

void VFS_Destroy(Plugin* plugin) {
  delete plugin;
}

bool VFS_ReadFile(Plugin* plugin,
                  LPVFSFUNCDATA lpVFSData,
                  PluginFile* file,
                  LPVOID lpData,
                  DWORD dwSize,
                  LPDWORD lpdwReadSize) {
  return plugin->ReadFile(file, std::span<uint8_t>(static_cast<uint8_t*>(lpData), dwSize), lpdwReadSize);
}

BOOL VFS_WriteFile(Plugin* data,
                   LPVFSFUNCDATA lpFuncData,
                   PluginFile* file,
                   LPVOID lpData,
                   DWORD dwSize,
                   BOOL fFlush,
                   LPDWORD lpdwWriteSize) {
  return data->WriteFile(file, std::span<uint8_t>(static_cast<uint8_t*>(lpData), dwSize), lpdwWriteSize);
}

void VFS_CloseFile(Plugin* plugin, LPVFSFUNCDATA lpVFSData, PluginFile* file) {
  plugin->CloseFile(file);
}

BOOL VFS_MoveFileW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszOldPath, LPTSTR lpszNewPath) {
  return plugin->MoveFile(lpszOldPath, lpszNewPath);
}

BOOL VFS_CreateDirectoryW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, DWORD dwFlags) {
  return plugin->CreateDir(lpszPath);
}

PluginFile* VFS_CreateFileW(Plugin* plugin,
                            LPVFSFUNCDATA lpVFSData,
                            LPWSTR lpszPath,
                            DWORD dwMode,
                            DWORD dwFileAttr,
                            DWORD dwFlags,
                            LPFILETIME lpFT) {
  return plugin->OpenFile(lpszPath, dwMode == GENERIC_WRITE);
}

bool VFS_ReadDirectoryW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
  return plugin->ReadDirectory(lpRDD);
}

int VFS_ContextVerbW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPVFSCONTEXTVERBDATAW lpVerbData) {
  return plugin->ContextVerb(lpVerbData);
}

UINT VFS_BatchOperationW(Plugin* plugin, LPVFSFUNCDATA lpVFSData, LPWSTR lpszPath, LPVFSBATCHDATAW lpBatchData) {
  return plugin->BatchOperation(lpszPath, lpBatchData);
}

bool VFS_PropGetW(Plugin* plugin,
                  vfsProperty propId,
                  LPVOID lpPropData,
                  LPVOID lpData1,
                  LPVOID lpData2,
                  LPVOID lpData3) {
  return plugin->PropGet(propId, lpPropData, lpData1, lpData2, lpData3);
}

bool VFS_GetFreeDiskSpaceW(Plugin* plugin,
                           LPVFSFUNCDATA lpFuncData,
                           LPWSTR lpszPath,
                           uint64_t* piFreeBytesAvailable,
                           uint64_t* piTotalBytes,
                           uint64_t* piTotalFreeBytes) {
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

PluginFindData* VFS_FindFirstFileW(Plugin* plugin,
                                   LPVFSFUNCDATA lpVFSData,
                                   LPWSTR lpszPath,
                                   LPWIN32_FIND_DATA lpwfdData,
                                   HANDLE hAbortEvent) {
  return plugin->FindFirst(lpszPath, lpwfdData, hAbortEvent);
}

BOOL VFS_FindNextFileW(Plugin* plugin,
                       LPVFSFUNCDATA lpVFSData,
                       PluginFindData* find_data,
                       LPWIN32_FIND_DATA lpwfdData) {
  return plugin->FindNext(find_data, lpwfdData);
}

void VFS_FindClose(Plugin* plugin, PluginFindData* find_data) {
  plugin->FindClose(find_data);
}

BOOL VFS_ExtractFilesW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPVFSEXTRACTFILESDATAW lpExtractData) {
  return plugin->ExtractEntries(lpFuncData, dopus::wstring_view_span(lpExtractData->lpszFiles),
                                lpExtractData->lpszDestPath);
}

long VFS_GetLastError(Plugin* data) {
  return data->GetError();
}

LPVFSFILEDATAHEADER VFS_GetFileInformationW(Plugin* data,
                                            LPVFSFUNCDATA lpVFSData,
                                            LPWSTR lpszPath,
                                            HANDLE hHeap,
                                            DWORD dwFlags) {
  return data->GetfileInformation(lpszPath, hHeap);
}

BOOL VFS_SetFileAttrW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, DWORD dwAttr, BOOL fForDelete) {
  return false;
}

BOOL VFS_SetFileCommentW(Plugin* plugin, LPVFSFUNCDATA lpFuncData, LPTSTR lpszPath, LPTSTR lpszComment) {
  return false;
}

BOOL VFS_SetFileTimeW(Plugin* plugin,
                      LPVFSFUNCDATA lpFuncData,
                      LPTSTR lpszPath,
                      LPFILETIME lpCreateTime,
                      LPFILETIME lpAccessTime,
                      LPFILETIME lpWriteTime) {
  return false;
}

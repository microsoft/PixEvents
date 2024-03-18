// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include <shlobj.h>
#include <strsafe.h>
#include <knownfolders.h>

// Defining PIX3_WIN_UNIT_TEST allows us to replace various Win32 APIs (used in pix3_win.h) with our own implementations
#define PIX3_WIN_UNIT_TEST

static std::function<BOOL(DWORD, LPCWSTR, HMODULE*)> g_getModuleHandleExImpl;
static std::function<HRESULT(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*)> g_sHGetKnownFolderPathImpl;
static std::function<void(LPVOID)> g_coTaskMemFreeImpl;
static std::function<HANDLE(LPCWSTR, LPWIN32_FIND_DATAW)> g_findFirstFileImpl;
static std::function<DWORD(LPCWSTR)> g_getFileAttributesImpl;
static std::function<BOOL(HANDLE, LPWIN32_FIND_DATAW)> g_findNextFileImpl;
static std::function<BOOL(HANDLE)> g_findCloseImpl;
static std::function<HMODULE(LPCWSTR, DWORD)> g_loadLibraryExImpl;

// These functions in PixImpl are normally defined in pix3_win.h, but we replace them with our own definitions
namespace PixImpl
{
    BOOL GetModuleHandleExW(
        DWORD dwFlags,
        LPCWSTR lpModuleName,
        HMODULE* phModule)
    {
        return g_getModuleHandleExImpl(dwFlags, lpModuleName, phModule);
    }

    HRESULT SHGetKnownFolderPath(
        REFKNOWNFOLDERID rfid,
        DWORD dwFlags,
        HANDLE hToken,
        PWSTR* ppszPath)
    {
        return g_sHGetKnownFolderPathImpl(rfid, dwFlags, hToken, ppszPath);
    }

    void CoTaskMemFree(LPVOID pv)
    {
        return g_coTaskMemFreeImpl(pv);
    }

    HANDLE FindFirstFileW(
        LPCWSTR lpFileName,
        LPWIN32_FIND_DATAW lpFindFileData)
    {
        return g_findFirstFileImpl(lpFileName, lpFindFileData);
    }

    DWORD GetFileAttributesW(LPCWSTR lpFileName)
    {
        return g_getFileAttributesImpl(lpFileName);
    }

    BOOL FindNextFileW(
        HANDLE hFindFile,
        LPWIN32_FIND_DATAW lpFindFileData)
    {
        return g_findNextFileImpl(hFindFile, lpFindFileData);
    }

    BOOL FindClose(HANDLE hFindFile)
    {
        return g_findCloseImpl(hFindFile);
    }

    HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, DWORD flags)
    {
        return g_loadLibraryExImpl(lpLibFileName, flags);
    }
}

#include <d3d12.h>
#include <pix3.h>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            g_getModuleHandleExImpl = nullptr;
            g_sHGetKnownFolderPathImpl = nullptr;
            g_coTaskMemFreeImpl = nullptr;
            g_findFirstFileImpl = nullptr;
            g_getFileAttributesImpl = nullptr;
            g_findNextFileImpl = nullptr;
            g_findCloseImpl = nullptr;
            g_loadLibraryExImpl = nullptr;
        }
    };
}

static void SetWin32FindDataAsDirectory(LPWIN32_FIND_DATAW findData, std::wstring_view fileName)
{
    wcsncpy(findData->cFileName, fileName.data(), fileName.size());
    findData->cFileName[fileName.size()] = L'\0';
    findData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
}

// If the DLL is already loaded, then PIXLoadLatestWinPixGpuCapturerLibrary should return that DLL
TEST(LoadLatestDllTests, DllAlreadyLoaded)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE* module) -> BOOL
    {
        *module = (HMODULE)42u;
        return TRUE;
    };

    EXPECT_EQ((HMODULE)42u, PIXLoadLatestWinPixGpuCapturerLibrary());
}

// If for some reason SHGetKnownFolderPath fails, then PIXLoadLatestWinPixGpuCapturerLibrary() should gracefully fail
TEST(LoadLatestDllTests, KnownFolderFails)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*) -> HRESULT
    {
        return E_FAIL;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    EXPECT_EQ(NULL, PIXLoadLatestWinPixGpuCapturerLibrary());
}

// If there aren't any PIX installations on the PC, then PIXLoadLatestWinPixGpuCapturerLibrary() should return nullptr
TEST(LoadLatestDllTests, NoPixInstallations)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) -> HRESULT
    {
        static std::wstring fakePath = L"x:\\something";
        *path = fakePath.data();
        return S_OK;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    g_findFirstFileImpl = [&](LPCWSTR path, LPWIN32_FIND_DATAW findData) -> HANDLE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\*");

        return INVALID_HANDLE_VALUE;
    };

    g_findCloseImpl = [&](HANDLE) -> BOOL
    {
        return TRUE;
    };

    EXPECT_EQ(nullptr, PIXLoadLatestWinPixGpuCapturerLibrary());
}

TEST(LoadLatestDllTests, OnlyOnePixInstallationFound)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) -> HRESULT
    {
        static std::wstring fakePath = L"x:\\something";
        *path = fakePath.data();
        return S_OK;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    g_findFirstFileImpl = [&](LPCWSTR path, LPWIN32_FIND_DATAW findData) -> HANDLE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\*");

        SetWin32FindDataAsDirectory(findData, L"PixInstallation000");
        return (HANDLE)1337u;
    };

    g_getFileAttributesImpl = [&](LPCWSTR path) -> DWORD
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\PixInstallation000\\WinPixGpuCapturer.dll");
        return 0u;
    };

    g_findNextFileImpl = [&](HANDLE, LPWIN32_FIND_DATAW findData) -> BOOL
    {
        return FALSE;
    };

    g_findCloseImpl = [&](HANDLE) -> BOOL
    {
        return TRUE;
    };

    g_loadLibraryExImpl = [&](LPCWSTR path, DWORD flags) -> HMODULE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\PixInstallation000\\WinPixGpuCapturer.dll");
        EXPECT_EQ(flags, (DWORD)LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        return (HMODULE)42u;
    };

    EXPECT_EQ((HMODULE)42u, PIXLoadLatestWinPixGpuCapturerLibrary());
}

// The user could have deleted WinPixGpuCapturer.dll, and we should gracefully handle this
TEST(LoadLatestDllTests, InstallationMissingWinPixGpuCapturer)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) -> HRESULT
    {
        static std::wstring fakePath = L"x:\\something";
        *path = fakePath.data();
        return S_OK;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    g_findFirstFileImpl = [&](LPCWSTR path, LPWIN32_FIND_DATAW findData) -> HANDLE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\*");

        SetWin32FindDataAsDirectory(findData, L"PixInstallation000");
        return (HANDLE)1337u;
    };

    g_getFileAttributesImpl = [&](LPCWSTR path) -> DWORD
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\PixInstallation000\\WinPixGpuCapturer.dll");
        return INVALID_FILE_ATTRIBUTES;
    };

    g_findNextFileImpl = [&](HANDLE, LPWIN32_FIND_DATAW findData) -> BOOL
    {
        return FALSE;
    };

    g_findCloseImpl = [&](HANDLE) -> BOOL
    {
        return TRUE;
    };

    EXPECT_EQ(nullptr, PIXLoadLatestWinPixGpuCapturerLibrary());
}

TEST(LoadLatestDllTests, MultiplePixInstallations_PicksAlphabeticallyLastOne)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) -> HRESULT
    {
        static std::wstring fakePath = L"x:\\something";
        *path = fakePath.data();
        return S_OK;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    g_findFirstFileImpl = [&](LPCWSTR path, LPWIN32_FIND_DATAW findData) -> HANDLE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\*");

        SetWin32FindDataAsDirectory(findData, L"PixInstallation000");
        return (HANDLE)1337u;
    };

    g_getFileAttributesImpl = [&](LPCWSTR path) -> DWORD
    {
        if (std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation000\\WinPixGpuCapturer.dll"
            || std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation007\\WinPixGpuCapturer.dll"
            || std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation002\\WinPixGpuCapturer.dll")
        {
            return 0u;
        }

        EXPECT_FALSE(true); // Have to use EXPECT here because FAIL() doesn't work in a lambda
        return 0u;
    };

    uint32_t numNextFileCalls = 0u;
    g_findNextFileImpl = [&](HANDLE, LPWIN32_FIND_DATAW findData) -> BOOL
    {
        numNextFileCalls++;

        // Note: we intentionally return results in non-alphabetical order, to make sure we still
        // pick the alphabetically last one
        switch (numNextFileCalls)
        {
        case 1u:
            SetWin32FindDataAsDirectory(findData, L"PixInstallation007"); 
            return TRUE;
        case 2u:
            SetWin32FindDataAsDirectory(findData, L"PixInstallation002");
            return TRUE;

        default:
            return FALSE;
        }
    };

    g_findCloseImpl = [&](HANDLE) -> BOOL
    {
        return TRUE;
    };

    g_loadLibraryExImpl = [&](LPCWSTR path, DWORD flags) -> HMODULE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\PixInstallation007\\WinPixGpuCapturer.dll");
        EXPECT_EQ(flags, (DWORD)LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        return (HMODULE)42u;
    };

    EXPECT_EQ((HMODULE)42u, PIXLoadLatestWinPixGpuCapturerLibrary());
}

// Same as the last test, but this sanity checks that the "WinPixTimingCapturer" version of the API works as expected too.
TEST(LoadLatestDllTests, MultiplePixInstallations_PicksAlphabeticallyLastOne_TimingCapturer)
{
    Fixture f;

    g_getModuleHandleExImpl = [&](DWORD, LPCWSTR, HMODULE*) -> BOOL
    {
        return FALSE;
    };

    g_sHGetKnownFolderPathImpl = [&](REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) -> HRESULT
    {
        static std::wstring fakePath = L"x:\\something";
        *path = fakePath.data();
        return S_OK;
    };

    g_coTaskMemFreeImpl = [&](LPVOID)
    {
    };

    g_findFirstFileImpl = [&](LPCWSTR path, LPWIN32_FIND_DATAW findData) -> HANDLE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\*");

        SetWin32FindDataAsDirectory(findData, L"PixInstallation000");
        return (HANDLE)1337u;
    };

    g_getFileAttributesImpl = [&](LPCWSTR path) -> DWORD
    {
        if (std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation000\\WinPixTimingCapturer.dll"
            || std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation007\\WinPixTimingCapturer.dll"
            || std::wstring(path) == L"x:\\something\\Microsoft PIX\\PixInstallation002\\WinPixTimingCapturer.dll")
        {
            return 0u;
        }

        EXPECT_FALSE(true); // Have to use EXPECT here because FAIL() doesn't work in a lambda
        return 0u;
    };

    uint32_t numNextFileCalls = 0u;
    g_findNextFileImpl = [&](HANDLE, LPWIN32_FIND_DATAW findData) -> BOOL
    {
        numNextFileCalls++;

        // Note: we intentionally return results in non-alphabetical order, to make sure we still
        // pick the alphabetically last one
        switch (numNextFileCalls)
        {
        case 1u:
            SetWin32FindDataAsDirectory(findData, L"PixInstallation007");
            return TRUE;
        case 2u:
            SetWin32FindDataAsDirectory(findData, L"PixInstallation002");
            return TRUE;

        default:
            return FALSE;
        }
    };

    g_findCloseImpl = [&](HANDLE) -> BOOL
    {
        return TRUE;
    };

    g_loadLibraryExImpl = [&](LPCWSTR path, DWORD flags) -> HMODULE
    {
        EXPECT_STREQ(path, L"x:\\something\\Microsoft PIX\\PixInstallation007\\WinPixTimingCapturer.dll");
        EXPECT_EQ(flags, (DWORD)(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR));
        return (HMODULE)42u;
    };

    EXPECT_EQ((HMODULE)42u, PIXLoadLatestWinPixTimingCapturerLibrary());
}

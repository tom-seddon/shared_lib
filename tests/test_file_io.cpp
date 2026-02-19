#include <shared/system.h>
#include <shared/file_io.h>
#include <shared/path.h>
#include <shared/testing.h>
#include <stdio.h>
#include <shared/system_specific.h>
#include <map>

#ifndef TEST_FILES_FOLDER
#error
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetPath(const std::string &name) {
    return PathJoined(TEST_FILES_FOLDER, name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CheckFile(const std::string &path, const std::vector<uint8_t> &wanted_data) {
    std::vector<uint8_t> got_data;
    TEST_TRUE(LoadFile(&got_data, path, nullptr));

    TEST_EQ_UU(got_data.size(), wanted_data.size());
    TEST_EQ_AA(got_data.data(), wanted_data.data(), wanted_data.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestBinary(size_t n) {
    std::vector<uint8_t> data;
    data.resize(n);

    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = (uint8_t)rand();
    }

    std::string buffer_path = GetPath("buffer.dat");
    printf("%s: %zu\n", buffer_path.c_str(), n);
    TEST_TRUE(SaveFile(data.data(), data.size(), buffer_path, nullptr, 0));

    std::string vector_path = GetPath("vector.dat");
    printf("%s: %zu\n", vector_path.c_str(), n);
    TEST_TRUE(SaveFile(data, vector_path, nullptr, 0));

    CheckFile(buffer_path, data);
    CheckFile(vector_path, data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// For now, don't exercise the newline translation - it defers to the CRT, so
// the behaviour is not consistent across platforms.
static void TestText(size_t n) {
    std::string wanted_data;
    wanted_data.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        wanted_data.push_back(32 + rand() % 95);
    }

    std::string path = GetPath("string.txt");
    printf("%s: %zu\n", path.c_str(), n);
    TEST_TRUE(SaveTextFile(wanted_data, path, nullptr));

    std::string got_string;
    TEST_TRUE(LoadTextFile(&got_string, path, nullptr));
    TEST_EQ_UU(got_string.size(), wanted_data.size());
    TEST_EQ_AA(got_string.data(), wanted_data.data(), wanted_data.size());
    TEST_EQ_SS(got_string.c_str(), wanted_data.c_str());

    std::vector<char> got_data;
    TEST_TRUE(LoadTextFile(&got_data, path, nullptr));
    TEST_EQ_UU(got_data.size(), wanted_data.size() + 1); //loaded data includes the terminator
    TEST_EQ_SS(got_data.data(), wanted_data.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestSize(size_t n) {
    TestBinary(n);
    TestText(n);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS

static WIN32_FIND_DATAA GetFindFirstFileData(const std::string &path_mbcs) {
    WIN32_FIND_DATAA fd;
    HANDLE find_h = FindFirstFileA(path_mbcs.c_str(), &fd);
    TEST_NE_UU((uintptr_t)find_h, (uintptr_t)INVALID_HANDLE_VALUE);

    TEST_TRUE(FindClose(find_h));
    find_h = INVALID_HANDLE_VALUE;

    return fd;
}

//static WIN32_FIND_DATAW GetFindFirstFileData(const std::wstring &path_wide) {
//    WIN32_FIND_DATAW fd;
//    HANDLE find_h = FindFirstFileW(path_wide.c_str(), &fd);
//    TEST_NE_UU((uintptr_t)find_h, (uintptr_t)INVALID_HANDLE_VALUE);
//
//    TEST_TRUE(FindClose(find_h));
//    find_h = INVALID_HANDLE_VALUE;
//
//    return fd;
//}

static std::string GetMBCS(const std::string &path_utf8) {
    std::wstring path_wide = GetWideString(path_utf8);
    TEST_EQ_SS(GetUTF8String(path_wide), path_utf8);
    std::string path_mbcs = GetByteString(path_wide, CP_THREAD_ACP);
    TEST_NE_SS(path_mbcs, path_utf8);

    // no wide version of string tests currently...
    std::wstring round_tripped_path_wide = GetWideString(path_mbcs, CP_THREAD_ACP);
    TEST_TRUE(round_tripped_path_wide == path_wide);

    return path_mbcs;
}

static void EnsureFileDeleted(const std::string &test_file_utf8) {
    if (PathIsFileOnDisk(test_file_utf8, nullptr, nullptr)) {
        TEST_TRUE(DeleteFileW(GetWideString(test_file_utf8).c_str()));
    }

    TEST_NULL(fopenUTF8(test_file_utf8.c_str(), "rb"));
}

static std::vector<uint8_t> GetRandomData() {
    std::vector<uint8_t> data;
    data.resize(100);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = 32 + rand() % 95;
    }

    return data;
}

static void MustFindPath(const std::string &path, const std::vector<std::string> &paths) {
    for (const std::string &path2 : paths) {
        if (PathCompare(path, path2) == 0) {
            return;
        }
    }

    TEST_FAIL("path not found: %s", path.c_str());
}

static void TestUTF8Names() {
    // U+00A3 POUND SIGN
    std::string test_folder_utf8 = PathJoined(TEST_FILES_FOLDER, (const char *)u8"\u00a3");
    std::string test_folder_mbcs = GetMBCS(test_folder_utf8);

    std::string test_file_utf8 = PathJoined(test_folder_utf8, (const char *)u8"\u00a3.dat");
    std::string test_file_mbcs = GetMBCS(test_file_utf8);

    // do this with Win32 stuff, to ensure it works
    {
        DWORD attributes = GetFileAttributesA(test_folder_mbcs.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            TEST_EQ_UU(GetLastError(), ERROR_FILE_NOT_FOUND);
        } else {
            TEST_TRUE(attributes & FILE_ATTRIBUTE_DIRECTORY);

            WIN32_FIND_DATAA fd;
            std::string pattern = test_folder_mbcs + "/*";
            HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
            TEST_NE_UU((uintptr_t)h, (uintptr_t)INVALID_HANDLE_VALUE);
            do {
                if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
                    // ignore...
                    TEST_TRUE(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                } else {
                    TEST_FALSE(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                    std::string file_path = test_folder_mbcs + "/" + fd.cFileName;
                    TEST_TRUE(DeleteFileA(file_path.c_str()));
                }
            } while (FindNextFileA(h, &fd));
            TEST_TRUE(FindClose(h));

            TEST_TRUE(RemoveDirectoryA(test_folder_mbcs.c_str()));
        }
    }

    TEST_FALSE(PathIsFolderOnDisk(test_folder_utf8));
    TEST_TRUE(PathCreateFolder(test_folder_utf8));
    TEST_TRUE(GetFindFirstFileData(test_folder_mbcs.c_str()).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    TEST_TRUE(PathIsFolderOnDisk(test_folder_utf8));

    // fopenUTF8 again
    {
        std::vector<uint8_t> wanted_data = GetRandomData();

        EnsureFileDeleted(test_file_utf8);

        FILE *fp = fopenUTF8(test_file_utf8.c_str(), "wb");
        TEST_NON_NULL(fp);
        TEST_EQ_UU(fwrite(wanted_data.data(), 1, wanted_data.size(), fp), wanted_data.size());
        fclose(fp), fp = nullptr;

        CheckFile(test_file_utf8.c_str(), wanted_data);
    }

    // SaveFile
    {
        std::vector<uint8_t> wanted_data = GetRandomData();

        EnsureFileDeleted(test_file_utf8);

        SaveFile(wanted_data, test_file_utf8, nullptr);

        CheckFile(test_file_utf8.c_str(), wanted_data);
    }

    // SaveTextFile
    {
        std::vector<uint8_t> wanted_data = GetRandomData();
        std::string wanted_string(wanted_data.begin(), wanted_data.end()); //the data is all ASCII chars in this case

        EnsureFileDeleted(test_file_utf8);

        SaveTextFile(wanted_string, test_file_utf8, nullptr);

        std::string got_string;
        TEST_TRUE(LoadTextFile(&got_string, test_file_utf8, nullptr));
        TEST_EQ_SS(got_string, wanted_string);
    }

    std::vector<std::string> paths;
    PathGlob(test_folder_utf8, [&paths](const std::string &path, bool is_folder) -> void {
        TEST_FALSE(is_folder);
        paths.push_back(path);
    });

    TEST_EQ_UU(paths.size(), 1);
    MustFindPath(test_file_utf8, paths);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
    for (size_t i = 0; i < 100; ++i) {
        TestSize(i);
    }

    for (size_t i = 7; i < 20; ++i) {
        TestSize((size_t)1 << i);
    }

#if SYSTEM_WINDOWS
    TestUTF8Names();
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/system.h>
#include <shared/file_io.h>
#include <shared/path.h>
#include <shared/testing.h>
#include <stdio.h>
#include <vector>
#include <shared/system_specific.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
#if SYSTEM_WINDOWS
    wchar_t *wfolder = _wgetenv(L"TMP");
    if (!wfolder) {
        wfolder = _wgetenv(L"TEMP");
    }
    TEST_NON_NULL(wfolder);

    std::string folder = GetUTF8String(wfolder);
#else
    std::string folder = "/tmp";
#endif

    std::string path = PathJoined(folder, "test_file_io_seek64.dat");

    static_assert(SIZE_MAX > UINT32_MAX);

    printf("Fill...\n");
    std::vector<uint8_t> wanted;
    wanted.resize((size_t)6 * 1024 * 1024 * 1024);
    wanted.back() = 255;

    printf("Save...\n");
    TEST_TRUE(SaveFile(wanted, path, nullptr));

    // this will exercise a certain amount of seek64/tell64
    printf("Reload...\n");
    std::vector<uint8_t> got;
    TEST_TRUE(LoadFile(&got, path, nullptr));

    printf("Coompare...\n");
    TEST_EQ_UU(wanted.size(), got.size());
    TEST_EQ_II(memcmp(wanted.data(), got.data(), got.size()), 0);

    printf("Rest...\n");
    FILE *fp = fopenUTF8(path.c_str(), "rb");
    TEST_NON_NULL(fp);

    TEST_EQ_II(fseek64(fp, wanted.size() - 1, SEEK_SET), 0);
    TEST_EQ_II(fgetc(fp), 255);
    TEST_EQ_II(ftell64(fp), got.size());

    fclose(fp), fp = nullptr;
}

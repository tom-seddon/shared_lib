#include <shared/system.h>
#include <shared/file_io.h>
#include <shared/path.h>
#include <shared/testing.h>
#include <stdio.h>

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

int main() {
    for (size_t i = 0; i < 100; ++i) {
        TestSize(i);
    }

    for (size_t i = 7; i < 20; ++i) {
        TestSize((size_t)1 << i);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/system.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <stdlib.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    (void)argc;

    std::string exe = PathGetEXEFileName();
    TEST_FALSE(exe.empty());

    printf("argv[0]: \"%s\"\n", argv[0]);
    printf("exe:     \"%s\"\n", exe.c_str());

    TEST_EQ_II(PathCompare(exe, argv[0]), 0);

#if SYSTEM_WINDOWS
    TEST_EQ_II(PathCompare("C:\\fred", "C:/FRED"), 0);
    TEST_TRUE(PathCompare("C:\\fred", "C:/FRED/FRED") < 0);
    TEST_TRUE(PathCompare("C:/FRED/FRED", "C:\\fred") > 0);
#endif

    TEST_EQ_SS(PathGetName("test.txt"), "test.txt");
    TEST_EQ_SS(PathGetName("/test.txt"), "test.txt");
    TEST_EQ_SS(PathGetName("a/test.txt"), "test.txt");
    TEST_EQ_SS(PathGetName("/a/test.txt"), "test.txt");
    TEST_EQ_SS(PathGetName("/a/b/c/d/test.txt"), "test.txt");

    TEST_EQ_SS(PathWithoutExtension("fred"), "fred");
    TEST_EQ_SS(PathWithoutExtension("fred.txt"), "fred");
    TEST_EQ_SS(PathWithoutExtension("folder/fred.txt"), "folder/fred");
    TEST_EQ_SS(PathWithoutExtension("folder.xyz/fred.txt"), "folder.xyz/fred");
    TEST_EQ_SS(PathWithoutExtension("folder.xyz/fred"), "folder.xyz/fred");
    TEST_EQ_SS(PathWithoutExtension("folder.xyz/"), "folder.xyz/");

    TEST_EQ_SS(PathGetFolder("fred"), "");
    TEST_EQ_SS(PathGetFolder("/fred"), "/");
    TEST_EQ_SS(PathGetFolder("folder/fred"), "folder/");
    TEST_EQ_SS(PathGetFolder("folder/"), "folder/");

    TEST_EQ_SS(PathJoined("a", "b"), "a" DEFAULT_SEPARATOR "b");
    TEST_EQ_SS(PathJoined("a/", "b"), "a/b");
    TEST_EQ_SS(PathJoined("a", "/b"), "/b");
    TEST_EQ_SS(PathJoined("a/", "/b"), "/b");

#if SYSTEM_WINDOWS
    TEST_EQ_SS(PathJoined("a", "D:\\test"), "D:\\test");
    TEST_EQ_SS(PathJoined("a", "D:/test"), "D:/test");
    TEST_EQ_SS(PathJoined("a", "D:test"), "D:test");
#endif

    TEST_EQ_SS(PathGetExtension("a.b"), ".b");
    TEST_EQ_SS(PathGetExtension("a.b/c.d"), ".d");
    TEST_EQ_SS(PathGetExtension("a.b/c.d.e"), ".e");
    TEST_EQ_SS(PathGetExtension("a.b/c"), "");
    TEST_EQ_SS(PathGetExtension("a.b/"), "");
    TEST_EQ_SS(PathGetExtension("a"), "");

    TEST_TRUE(PathIsFolderOnDisk(TEST_PATH_SOURCE_FOLDER));
    TEST_FALSE(PathIsFileOnDisk(TEST_PATH_SOURCE_FOLDER, nullptr, nullptr));

    std::string file = PathJoined(TEST_PATH_SOURCE_FOLDER, PathGetName(__FILE__));
    TEST_TRUE(PathIsFileOnDisk(file, nullptr, nullptr));
    TEST_FALSE(PathIsFolderOnDisk(file));

    PathGlob(PathGetFolder(__FILE__), [](const std::string &path, bool is_folder) -> void {
        (void)is_folder;
        std::string name = PathGetName(path);
        TEST_NE_SS(name, ".");
        TEST_NE_SS(name, "..");
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

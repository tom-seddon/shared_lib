#include <shared/system.h>
#include <shared/guid.h>
#include <shared/testing.h>

int main() {
    Guid tmp{0x33, 0x8a, 0x38, 0xad, 0x1e, 0x05, 0x4c, 0xa2, 0x90, 0x2b, 0x45, 0xa7, 0xd2, 0xed, 0x10, 0x2a};

    {
        Guid tmp2{0x33, 0x8a, 0x38, 0xad, 0x1e, 0x05, 0x4c, 0xa2, 0x90, 0x2b, 0x45, 0xa7, 0xd2, 0xed, 0x10, 0x2a};

        TEST_TRUE(tmp == tmp2);
        TEST_FALSE(tmp != tmp2);
        TEST_FALSE(tmp < tmp2);

        ++tmp2.bytes[0];

        TEST_FALSE(tmp == tmp2);
        TEST_TRUE(tmp != tmp2);
        TEST_TRUE(tmp < tmp2);

        tmp2.bytes[0] -= 2;

        TEST_TRUE(tmp2 < tmp);
    }

    char str[GUID_STR_SIZE];
    GetStringFromGuid(str, tmp);
    TEST_EQ_SS(str, "{338a38ad-1e05-4ca2-902b-45a7d2ed102a}");

    Guid tmp2;
    TEST_TRUE(GetGuidFromString(&tmp2, str));
    TEST_TRUE(tmp == tmp2);

    tmp = GetGuidFromDEFINE_GUID(0xbebfc583, 0x1ec6, 0x4233, 0xaf, 0xf2, 0xa5, 0xcc, 0x51, 0x69, 0xca, 0x87);
    TEST_EQ_UU(tmp.bytes[0], 0xbe);
    TEST_EQ_UU(tmp.bytes[1], 0xbf);
    TEST_EQ_UU(tmp.bytes[2], 0xc5);
    TEST_EQ_UU(tmp.bytes[3], 0x83);

    TEST_EQ_UU(tmp.bytes[4], 0x1e);
    TEST_EQ_UU(tmp.bytes[5], 0xc6);

    TEST_EQ_UU(tmp.bytes[6], 0x42);
    TEST_EQ_UU(tmp.bytes[7], 0x33);

    TEST_EQ_UU(tmp.bytes[8], 0xaf);
    TEST_EQ_UU(tmp.bytes[9], 0xf2);
    TEST_EQ_UU(tmp.bytes[10], 0xa5);
    TEST_EQ_UU(tmp.bytes[11], 0xcc);
    TEST_EQ_UU(tmp.bytes[12], 0x51);
    TEST_EQ_UU(tmp.bytes[13], 0x69);
    TEST_EQ_UU(tmp.bytes[14], 0xca);
    TEST_EQ_UU(tmp.bytes[15], 0x87);
}

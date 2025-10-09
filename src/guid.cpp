#include <shared/system.h>
#include <shared/guid.h>
#include <shared/debug.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <shared/load_store.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Guid GetGuidFromDEFINE_GUID(uint32_t data1, uint16_t data2, uint16_t data3, uint8_t data4_0, uint8_t data4_1, uint8_t data4_2, uint8_t data4_3, uint8_t data4_4, uint8_t data4_5, uint8_t data4_6, uint8_t data4_7) {
    Guid guid;

    Store32BE(&guid.bytes[0], data1);
    Store16BE(&guid.bytes[4], data2);
    Store16BE(&guid.bytes[6], data3);
    guid.bytes[8] = data4_0;
    guid.bytes[9] = data4_1;
    guid.bytes[10] = data4_2;
    guid.bytes[11] = data4_3;
    guid.bytes[12] = data4_4;
    guid.bytes[13] = data4_5;
    guid.bytes[14] = data4_6;
    guid.bytes[15] = data4_7;

    return guid;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GetStringFromGuid(char *str, const Guid &guid) {
    char *c = str;

    *c++ = '{';
    *c++ = HEX_CHARS_LC[guid.bytes[0] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[0] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[1] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[1] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[2] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[2] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[3] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[3] & 0xf];
    *c++ = '-';
    *c++ = HEX_CHARS_LC[guid.bytes[4] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[4] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[5] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[5] & 0xf];
    *c++ = '-';
    *c++ = HEX_CHARS_LC[guid.bytes[6] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[6] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[7] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[7] & 0xf];
    *c++ = '-';
    *c++ = HEX_CHARS_LC[guid.bytes[8] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[8] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[9] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[9] & 0xf];
    *c++ = '-';
    *c++ = HEX_CHARS_LC[guid.bytes[10] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[10] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[11] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[11] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[12] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[12] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[13] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[13] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[14] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[14] & 0xf];
    *c++ = HEX_CHARS_LC[guid.bytes[15] >> 4];
    *c++ = HEX_CHARS_LC[guid.bytes[15] & 0xf];
    *c++ = '}';

    ASSERT(c - str == GUID_STR_LEN);
    *c++ = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int GetNybble(const char **c) {
    char ch = **c;
    ++*c;

    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    } else {
        return -1;
    }
}

static bool GetByte(uint8_t *value, const char **c) {
    int h = GetNybble(c);
    if (h < 0) {
        return false;
    }

    int l = GetNybble(c);
    if (l < 0) {
        return false;
    }

    *value = (uint8_t)(l | h << 4);
    return true;
}

// {207af3bc-e5f3-4660-8278-6f4dd93f3b47}

bool GetGuidFromString(Guid *guid, const char *str) {
    const char *c = str;

    if (*c++ != '{') {
        return false;
    }

    int i = 0;

    if (!GetByte(&guid->bytes[i++], &c) ||
        !GetByte(&guid->bytes[i++], &c) ||
        !GetByte(&guid->bytes[i++], &c) ||
        !GetByte(&guid->bytes[i++], &c)) {
        return false;
    }

    while (i < 10) {
        if (*c++ != '-') {
            return false;
        }

        if (!GetByte(&guid->bytes[i++], &c) || !GetByte(&guid->bytes[i++], &c)) {
            return false;
        }
    }

    if (*c++ != '-') {
        return false;
    }

    while (i < 16) {
        if (!GetByte(&guid->bytes[i++], &c)) {
            return false;
        }
    }

    ASSERT(i == 16);

    if (*c++ != '}') {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator<(const Guid &a, const Guid &b) {
    return memcmp(&a, &b, sizeof(Guid)) < 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator==(const Guid &a, const Guid &b) {
    return memcmp(&a, &b, sizeof(Guid)) == 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

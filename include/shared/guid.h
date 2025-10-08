#ifndef HEADER_93BB1E8EC8A444A2B42D4503076A2EDD // -*- mode:c++ -*-
#define HEADER_93BB1E8EC8A444A2B42D4503076A2EDD

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// GUIDs. Supply data in big-endian format. The output of Python's
// uuid.uuid4().bytes is suitable.

struct Guid {
    uint8_t bytes[16] = {};
};
static_assert(sizeof(Guid) == 16);

bool operator<(const Guid &a, const Guid &b);
bool operator==(const Guid &a, const Guid &b);

inline bool operator!=(const Guid &a, const Guid &b) {
    return !(a == b);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Use forDEFINE_GUID-style data. The GUID string output of MS's guidgen tool
// seems to be no good - it looks like a version 4 UID, but the word values are
// treated as-is, so the version field ends up in the wrong place.
Guid GetGuidFromDEFINE_GUID(uint32_t data1, uint16_t data2, uint16_t data3, uint8_t data4_0, uint8_t data4_1, uint8_t data4_2, uint8_t data4_3, uint8_t data4_4, uint8_t data4_5, uint8_t data4_6, uint8_t data4_7);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// 0         1         2         3
// 01234567890123456789012345678901234567
// {207af3bc-e5f3-4660-8278-6f4dd93f3b47}

static constexpr size_t GUID_STR_LEN = 38;
static constexpr size_t GUID_STR_SIZE = GUID_STR_LEN + 1;

void GetStringFromGuid(char *str, const Guid &guid);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

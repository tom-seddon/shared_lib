#include <shared/system.h>
#include <shared/enums.h>
#include <shared/debug.h>
#include <atomic>
#include <string>
#include <shared/sha1.h>
#include <inttypes.h>
#include <shared/log.h>
#include <optional>
#include <shared/system_specific.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// set once anything examines the enums list - at which point it becomes fixed and can't be modified.
//
// this is a rather crude way of trying to ensure that all the EnumTraits objects are static globals.
static std::atomic<bool> g_enums_list_fixed{false};

static std::atomic<bool> g_enums_list_initialised{false};

static const EnumTraitsBase *g_first_enum_traits;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EnumValue::EnumValue(const EnumTraitsBase *traits_, const char *name_, uint64_t value_)
    : traits(traits_)
    , name(name_)
    , value(value_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EnumValue::EnumValue(const EnumTraitsBase *traits_, const char *name_, uint64_t value_, int8_t bit_shift_, uint8_t bit_width_)
    : traits(traits_)
    , name(name_)
    , value(value_)
    , bit_shift(bit_shift_)
    , bit_width(bit_width_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EnumTraitsBase::EnumTraitsBase() {
    ASSERT(!g_enums_list_fixed.load(std::memory_order_acquire));

    m_next_enum_traits = g_first_enum_traits;
    g_first_enum_traits = this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const EnumTraitsBase *EnumTraitsBase::GetFirst() {
    EnsureEnumsInitialised();

    g_enums_list_fixed.store(true, std::memory_order_release);
    return g_first_enum_traits;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const EnumTraitsBase *EnumTraitsBase::GetNext() const {
    return m_next_enum_traits;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetAllValues(const EnumValue *value, void *context) {
    auto str = (std::string *)context;

    *str += value->name;
    *str += "\n";

    // The actual value is irrelevant. Only the set of names is important.
}

struct CheckCountValueState {
    uint64_t ucount = 0, umax = 0;
    int64_t icount = 0, imax = 0;
    bool seen_count = false;
};

static void CheckCountValue(const EnumValue *value, void *context) {
    auto state = (CheckCountValueState *)context;

    if (value->traits->IsSigned()) {
        if (state->seen_count) {
            ASSERT((int64_t)value < state->imax);
        } else {
            if (strcmp(value->name, "Count") == 0 || strcmp(value->name, "MaxValue") == 0) {
                state->icount = (int64_t)value->value;
                state->seen_count = true;
            } else {
                state->imax = (std::max)(state->imax, (int64_t)value->value);
            }
        }
    } else {
        if (state->seen_count) {
            ASSERT(value->value < state->umax);
        } else {
            if (strcmp(value->name, "Count") == 0 || strcmp(value->name, "MaxValue") == 0) {
                state->ucount = value->value;
                state->seen_count = true;
            } else {
                state->umax = (std::max)(state->umax, value->value);
            }
        }
    }
}

struct CheckBitfieldsState {
    std::optional<bool> is_all_bitfields;
};

static void CheckBitfields(const EnumValue *value, void *context) {
    auto state = (CheckBitfieldsState *)context;

    bool is_bitfield;
    if (value->bit_shift < 0) {
        ASSERT(value->bit_width == 0);
        is_bitfield = false;
    } else {
        ASSERT(value->bit_width > 0);
        is_bitfield = true;
    }

    if (state->is_all_bitfields.has_value()) {
        ASSERT(is_bitfield == *state->is_all_bitfields);
    } else {
        state->is_all_bitfields = is_bitfield;
    }
}

static void PrintEnumValue(const EnumValue *value, void *context) {
    (void)context;

    printf("    %s: ", value->name);

    if (value->traits->IsSigned()) {
        printf("%" PRId64, (int64_t)value->value);
    } else {
        printf("%" PRIu64 " (0x%" PRIx64 ")", value->value, value->value);
    }

    printf("\n");
}

void EnsureEnumsInitialised() {
    if (g_enums_list_initialised.exchange(true, std::memory_order_release)) {
        return;
    }

    bool all_good = true;
    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->GetNext()) {
        // Check serializable hash.
        if (const char *got_hash = traits->GetSerializableHash()) {
            std::string stuff;
            traits->ForEach(&GetAllValues, &stuff);

            char wanted_hash[SHA1::DIGEST_STR_SIZE];
            SHA1::HashBuffer(nullptr, wanted_hash, stuff.data(), stuff.size());

            if (strcmp(wanted_hash, got_hash) != 0) {
                char *msg;
                asprintf(&msg,
                         PRIfileline " %s: serializable hash: wanted \"%s\", got \"%s\"\n",
                         traits->GetEENDFile(),
                         traits->GetEENDLine(),
                         traits->GetEnumName(),
                         wanted_hash,
                         got_hash);

                fputs(msg, stdout);
#if SYSTEM_WINDOWS
                OutputDebugStringA(msg);
#endif

                free(msg), msg = nullptr;

                all_good = false;
            }
        }

        // Make sure Count and MaxValue value make sense.
        CheckCountValueState ccvs;
        traits->ForEach(&CheckCountValue, &ccvs);

        // Ensure bitfields are consistent.
        CheckBitfieldsState cbs;
        traits->ForEach(&CheckBitfields, &cbs);
    }

    (void)all_good;
    ASSERT(all_good);

    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->GetNext()) {
        printf("%s: size=%zu signed=%s serializable=%s\n",
               traits->GetEnumName(),
               traits->GetSizeBytes(),
               BOOL_STR(traits->IsSigned()),
               BOOL_STR(traits->GetSerializableHash()));

        traits->ForEach(&PrintEnumValue, nullptr);
    }
}

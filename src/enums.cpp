#include <shared/system.h>
#include <shared/enums.h>
#include <shared/debug.h>
#include <atomic>
#include <string>
#include <shared/sha1.h>
#include <inttypes.h>

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

static void GetAllValues(uint64_t value, const char *name, const EnumTraitsBase *traits, void *context) {
    (void)traits;
    auto str = (std::string *)context;

    *str += name;
    *str += "\n";
    *str += std::to_string(value);
    *str += "\n";
}

struct CheckCountValueState {
    uint64_t ucount = 0, umax = 0;
    int64_t icount = 0, imax = 0;
    bool seen_count = false;
};

static void CheckCountValue(uint64_t value, const char *name, const EnumTraitsBase *traits, void *context) {
    auto state = (CheckCountValueState *)context;

    if (traits->IsSigned()) {
        if (state->seen_count) {
            ASSERT((int64_t)value < state->imax);
        } else {
            if (strcmp(name, "Count") == 0 || strcmp(name, "MaxValue") == 0) {
                state->icount = (int64_t)value;
                state->seen_count = true;
            } else {
                state->imax = (std::max)(state->imax, (int64_t)value);
            }
        }
    } else {
        if (state->seen_count) {
            ASSERT(value < state->umax);
        } else {
            if (strcmp(name, "Count") == 0 || strcmp(name, "MaxValue") == 0) {
                state->ucount = value;
                state->seen_count = true;
            } else {
                state->umax = (std::max)(state->umax, value);
            }
        }
    }
}

void EnsureEnumsInitialised() {
    if (g_enums_list_initialised.exchange(true, std::memory_order_release)) {
        return;
    }

    bool all_good = true;
    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->GetNext()) {
        if (const char *got_hash = traits->GetSerializableHash()) {
            std::string stuff;
            traits->ForEach(&GetAllValues, &stuff);

            char wanted_hash[SHA1::DIGEST_STR_SIZE];
            SHA1::HashBuffer(nullptr, wanted_hash, stuff.data(), stuff.size());

            if (strcmp(wanted_hash, got_hash) != 0) {
                printf("%s:%" PRId64 ": %s: serializable hash: wanted \"%s\", got \"%s\"\n",
                       traits->GetEENDFile(),
                       traits->GetEENDLine(),
                       traits->GetEnumName(), wanted_hash, got_hash);
                all_good = false;
            }
        }

        CheckCountValueState ccs;
        traits->ForEach(&CheckCountValue, &ccs);
    }

    (void)all_good;
    ASSERT(all_good);
}

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

EnumValue::EnumValue(int8_t bit_shift_, uint8_t bit_width_)
    : bit_shift(bit_shift_)
    , bit_width(bit_width_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EnumTraitsBase::EnumTraitsBase() {
    ASSERT(!g_enums_list_fixed.load(std::memory_order_acquire));

    this->next = g_first_enum_traits;
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

void EnsureEnumsInitialised() {
    if (g_enums_list_initialised.exchange(true, std::memory_order_release)) {
        return;
    }

    bool all_good = true;
    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->next) {
        // Check serializable hash.
        if (traits->serializable_hash) {
            std::string stuff;
            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                stuff += value->name;
                stuff += "\n";
            }

            char wanted_hash[SHA1::DIGEST_STR_SIZE];
            SHA1::HashBuffer(nullptr, wanted_hash, stuff.data(), stuff.size());

            if (strcmp(wanted_hash, traits->serializable_hash) != 0) {
                char *msg;
                asprintf(&msg,
                         PRIfileline " %s: serializable hash: wanted \"%s\", got \"%s\"\n",
                         traits->eend_file,
                         traits->eend_line,
                         traits->name,
                         wanted_hash,
                         traits->serializable_hash);

                fputs(msg, stdout);
#if SYSTEM_WINDOWS
                OutputDebugStringA(msg);
#endif

                free(msg), msg = nullptr;

                all_good = false;
            }
        }

        // Make sure Count and MaxValue value make sense.
        {
            uint64_t ucount = 0, umax = 0;
            int64_t icount = 0, imax = 0;
            bool seen_count = false;

            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                if (value->traits->is_signed) {
                    if (seen_count) {
                        (void)icount;
                        ASSERT((int64_t)value < icount);
                    } else {
                        if (strcmp(value->name, "Count") == 0 || strcmp(value->name, "MaxValue") == 0) {
                            icount = (int64_t)value->value;
                            seen_count = true;
                        } else {
                            imax = (std::max)(imax, (int64_t)value->value);
                        }
                    }
                } else {
                    if (seen_count) {
                        (void)ucount;
                        ASSERT(value->value < ucount);
                    } else {
                        if (strcmp(value->name, "Count") == 0 || strcmp(value->name, "MaxValue") == 0) {
                            ucount = value->value;
                            seen_count = true;
                        } else {
                            umax = (std::max)(umax, value->value);
                        }
                    }
                }
            }
        }

        // Check the enum is all bitfields, or all values.
        {
            std::optional<bool> is_all_bitfields;

            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                bool is_bitfield;
                if (value->bit_shift < 0) {
                    ASSERT(value->bit_width == 0);
                    is_bitfield = false;
                } else {
                    ASSERT(value->bit_width > 0);
                    is_bitfield = true;
                }

                if (is_all_bitfields.has_value()) {
                    ASSERT(is_bitfield == *is_all_bitfields);
                } else {
                    is_all_bitfields = is_bitfield;
                }
            }
        }

        // Check the bitfields are in bounds for the enum's size.
        {
            size_t size_bits = traits->size_bytes * 8;
            (void)size_bits;

            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                if (value->bit_shift >= 0) {
                    ASSERT(value->bit_width > 0);
                    ASSERT((size_t)value->bit_shift < size_bits);
                    ASSERT((size_t)(value->bit_shift + value->bit_width) <= size_bits);
                }
            }
        }
    }

    (void)all_good;
    ASSERT(all_good);

    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->next) {
        printf("%s: size=%zu signed=%s serializable=%s\n",
               traits->name,
               traits->size_bytes,
               BOOL_STR(traits->is_signed),
               BOOL_STR(traits->serializable_hash));

        for (const EnumValue *value = traits->first_value; value; value = value->next) {
            printf("    %s: ", value->name);

            if (value->traits->is_signed) {
                printf("%" PRId64, (int64_t)value->value);
            } else {
                printf("%" PRIu64 " (0x%" PRIx64 ")", value->value, value->value);
            }

            printf("\n");
        }
    }
}

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
#include <string.h>
#include <algorithm>
#include <vector>

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

EnumValue::EnumValue(int8_t bit_shift_, uint8_t bit_width_, const EnumTraitsBase *bit_enum_)
    : bit_shift(bit_shift_)
    , bit_width(bit_width_)
    , bit_enum(bit_enum_) {
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

void EnumTraitsBase::MustBeUninitialized() {
    ASSERT(!this->first_value);
    //printf("EnumTraitsBase::MustBeUninitialized: %s\n", this->name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void EnumTraitsBase::Init1(const char *name_, bool is_signed_, size_t size_bytes_, const char *serializable_hash_, int eend_line_, const char *eend_file_) {
    this->name = name_;
    this->is_signed = is_signed_;
    this->size_bits = size_bytes_ * CHAR_BIT;
    this->serializable_hash = serializable_hash_;
    this->eend_line = eend_line_;
    this->eend_file = eend_file_;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void EnumTraitsBase::Init2() {
    // Set up the bitfield flag. Don't check for consistency at this point; that can come later.
    ASSERT(this->first_value);
    for (const EnumValue *value = this->first_value; value; value = value->next) {
        if (value->bit_width > 0) {
            this->is_bitfield = true;
        }
    }
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
        for (const EnumValue *value = traits->first_value; value; value = value->next) {
            ASSERT(value->traits == traits);
        }

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
                if (asprintf(&msg,
                             PRIfileline " %s: serializable hash: wanted \"%s\", got \"%s\"\n",
                             traits->eend_file,
                             traits->eend_line,
                             traits->name,
                             wanted_hash,
                             traits->serializable_hash) != -1) {

                    fputs(msg, stdout);
#if SYSTEM_WINDOWS
                    OutputDebugStringA(msg);
#endif

                    free(msg), msg = nullptr;
                }

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
            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                if (value->bit_shift < 0) {
                    ASSERT(value->bit_width == 0);
                    ASSERT(!traits->is_bitfield);
                } else {
                    ASSERT(value->bit_width > 0);
                    ASSERT(traits->is_bitfield);
                }
            }
        }

        if (traits->is_bitfield) {
            ASSERT(!traits->is_signed);

            const EnumValue *bits[64] = {};

            for (const EnumValue *value = traits->first_value; value; value = value->next) {
                // Ensure field is within bounds.
                ASSERT((size_t)value->bit_shift < traits->size_bits);
                ASSERT((size_t)(value->bit_shift + value->bit_width) <= traits->size_bits);

                // Ensure field doesn't overlap with any other.
                uint64_t field_mask = (uint64_t)1 << value->bit_width;
                --field_mask;
                field_mask <<= value->bit_shift;
                (void)field_mask;

                ASSERT(value->value == field_mask);

                for (uint8_t i = 0; i < value->bit_width; ++i) {
                    const EnumValue **bit = &bits[value->bit_shift + i];
                    ASSERT(!*bit);
                    *bit = value;
                }
            }
        }
    }

    (void)all_good;
    ASSERT(all_good);

    std::vector<const EnumTraitsBase *> all_traits;
    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->next) {
        all_traits.push_back(traits);
    }

    std::sort(all_traits.begin(), all_traits.end(),
              [](const EnumTraitsBase *a, const EnumTraitsBase *b) -> bool {
                  return strcmp(a->name, b->name) < 0;
              });

    for (const EnumTraitsBase *traits : all_traits) {
        printf("%s: size=%zu signed=%s serializable=%s\n",
               traits->name,
               traits->size_bits,
               BOOL_STR(traits->is_signed),
               BOOL_STR(traits->serializable_hash));

        for (const EnumValue *value = traits->first_value; value; value = value->next) {
            printf("    %s: ", value->name);

            if (value->bit_width > 0) {
                printf("bit=%d width=%u (mask=0x%" PRIx64 ")", value->bit_shift, value->bit_width, value->value);
            } else {
                if (value->traits->is_signed) {
                    printf("%" PRId64, (int64_t)value->value);
                } else {
                    printf("%" PRIu64 " (0x%" PRIx64 ")", value->value, value->value);
                }
            }

            printf("\n");
        }
    }

    char msg[1000];
    snprintf(msg, sizeof msg, "%zu enums\n", all_traits.size());
    fputs(msg, stdout);
}

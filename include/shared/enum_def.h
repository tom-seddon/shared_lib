#include <type_traits>

#if EINTERNAL
#define EPREFIX static
#else
#define EPREFIX
#endif

#ifdef __cplusplus
#define EFALLTHROUGH [[fallthrough]]
#else
#define EFALLTHROUGH \
    BEGIN_MACRO {    \
    }                \
    END_MACRO
#endif

#define EEND_EXTRA()

#define EBEGIN__BODY(TYPE)                                                                                       \
    const typename EnumTraits<ENAME>::GetNameFn EnumTraits<ENAME>::GET_NAME_FN = &CONCAT3(Get, ENAME, EnumName); \
    const char EnumTraits<ENAME>::NAME[] = STRINGIZE(ENAME);                                                     \
    static const char *CONCAT3(InternalGet, ENAME, EnumName)(TYPE value,                                         \
                                                             const EnumValue **first_value_ptr) {                \
        static_assert(sizeof(TYPE) <= sizeof(uint64_t));                                                         \
                                                                                                                 \
        static EnumValue *s_first_value;                                                                         \
        EnumValue *prev_value = nullptr;                                                                         \
                                                                                                                 \
        if (first_value_ptr) {                                                                                   \
            if (s_first_value) {                                                                                 \
            return_first_value:                                                                                  \
                *first_value_ptr = s_first_value;                                                                \
                return nullptr;                                                                                  \
            } else {                                                                                             \
                goto fall_through;                                                                               \
            }                                                                                                    \
        }                                                                                                        \
                                                                                                                 \
        switch (value) {                                                                                         \
        fall_through:

#define EBEGIN() EBEGIN__BODY(int)
#define EBEGIN_DERIVED(BASE_NAME) EBEGIN__BODY(BASE_NAME)

#define EN_INTERNAL(NAME, STR, ...)                        \
    case (NAME):                                           \
        if (!first_value_ptr) {                            \
            return (STR);                                  \
        }                                                  \
        {                                                  \
            static EnumValue s_value{__VA_ARGS__};         \
            s_value.traits = &EnumTraits<ENAME>::s_traits; \
            s_value.name = (STR);                          \
            s_value.value = (uint64_t)(int64_t)(NAME);     \
            if (!s_first_value) {                          \
                s_first_value = &s_value;                  \
            } else {                                       \
                prev_value->next = &s_value;               \
            }                                              \
            prev_value = &s_value;                         \
        }                                                  \
        EFALLTHROUGH;

#define EN(NAME) EN_INTERNAL(NAME, #NAME)

#define EPN(NAME) EN_INTERNAL(CONCAT3(ENAME, _, NAME), STRINGIZE(NAME))

#define EPN_BIT_FLAG(NAME, BIT) \
    EN_INTERNAL(CONCAT3(ENAME, _, NAME), STRINGIZE(NAME), (BIT), 1)

#define ENV(NAME, VALUE) EN(NAME)

#define EPNV(NAME, VALUE) EPN(NAME)

#define EQN(NAME)
#define EQNV(NAME, VALUE)
#define EQPN(NAME)
#define EQPNV(NAME, VALUE)

#define EEND__BODY(SERIALIZABLE_HASH, EEND_FILE, EEND_LINE)                                                    \
    default:                                                                                                   \
        if (!first_value_ptr) {                                                                                \
            return "?" STRINGIZE(ENAME) "?";                                                                   \
        }                                                                                                      \
        }                                                                                                      \
                                                                                                               \
        /* only gets here for first time call for retrieving first value ptr. */                               \
        goto return_first_value;                                                                               \
        }                                                                                                      \
                                                                                                               \
        EPREFIX UNUSED const char *CONCAT3(Get, ENAME, EnumName)(typename EnumTraits<ENAME>::BaseType value) { \
            return CONCAT3(InternalGet, ENAME, EnumName)(value, nullptr);                                      \
        }                                                                                                      \
                                                                                                               \
        EnumTraits<ENAME>::EnumTraits() {                                                                      \
            this->name = NAME;                                                                                 \
            this->is_signed = std::is_signed<BaseType>::value;                                                 \
            this->size_bytes = sizeof(ENAME);                                                                  \
            this->serializable_hash = SERIALIZABLE_HASH;                                                       \
            this->eend_line = EEND_LINE;                                                                       \
            this->eend_file = EEND_FILE;                                                                       \
            CONCAT3(InternalGet, ENAME, EnumName)({}, &this->first_value);                                     \
        }                                                                                                      \
                                                                                                               \
        const EnumTraits<ENAME> EnumTraits<ENAME>::s_traits;                                                   \
                                                                                                               \
        EEND_EXTRA()

#define EEND() EEND__BODY(nullptr, __FILE__, __LINE__)
#define EEND_SERIALIZABLE(HASH) EEND__BODY(HASH, __FILE__, __LINE__)

/* This gets a bit freakish. */
#define NBEGIN(NAME)                                                \
    EPREFIX const char *CONCAT3(Get, NAME, EnumName)(ENAME value) { \
        switch (value) {                                            \
        default:                                                    \
            return "?" STRINGIZE(NAME) "?";

#define NN(NAME) \
    case (NAME): \
        return #NAME;
#define NNS(NAME, STR) \
    case (NAME):       \
        return (STR);

#define NEND() \
    }          \
    }

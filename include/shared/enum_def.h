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

#define EBEGIN__BODY(TYPE)                                                                                                               \
    const typename EnumTraits<ENAME>::GetNameFn EnumTraits<ENAME>::GET_NAME_FN = &CONCAT3(Get, ENAME, EnumName);                         \
    const char EnumTraits<ENAME>::NAME[] = STRINGIZE(ENAME);                                                                             \
    static const char *CONCAT3(InternalGet, ENAME, EnumName)(TYPE value, void (*fn)(uint64_t, const char *, void *), void *fn_context) { \
        static_assert(sizeof(TYPE) <= sizeof(uint64_t));                                                                                 \
        if (fn) {                                                                                                                        \
            goto fall_through;                                                                                                           \
        }                                                                                                                                \
                                                                                                                                         \
        switch (value) {                                                                                                                 \
        fall_through:

#define EBEGIN() EBEGIN__BODY(int)
#define EBEGIN_DERIVED(BASE_NAME) EBEGIN__BODY(BASE_NAME)

#define EN_INTERNAL(NAME, STR)                               \
    case (NAME):                                             \
        if (!fn) {                                           \
            return (STR);                                    \
        }                                                    \
        (*fn)((uint64_t)(int64_t)(NAME), (STR), fn_context); \
        EFALLTHROUGH;

#define EN(NAME) EN_INTERNAL(NAME, #NAME)

#define EPN(NAME) EN_INTERNAL(CONCAT3(ENAME, _, NAME), STRINGIZE(NAME))

#define ENV(NAME, VALUE) EN(NAME)

#define EPNV(NAME, VALUE) EPN(NAME)

#define EQN(NAME)
#define EQNV(NAME, VALUE)
#define EQPN(NAME)
#define EQPNV(NAME, VALUE)

#define EEND__BODY()                                                                                           \
    default:                                                                                                   \
        if (!fn) {                                                                                             \
            return "?" STRINGIZE(ENAME) "?";                                                                   \
        }                                                                                                      \
        }                                                                                                      \
                                                                                                               \
        return nullptr; /* only gets here if calling with a callback */                                        \
        }                                                                                                      \
                                                                                                               \
        EPREFIX UNUSED const char *CONCAT3(Get, ENAME, EnumName)(typename EnumTraits<ENAME>::BaseType value) { \
            return CONCAT3(InternalGet, ENAME, EnumName)(value, nullptr, nullptr);                             \
        }                                                                                                      \
                                                                                                               \
        const char *EnumTraits<ENAME>::GetEnumName() const {                                                   \
            return NAME;                                                                                       \
        }                                                                                                      \
                                                                                                               \
        bool EnumTraits<ENAME>::IsSigned() const {                                                             \
            return std::is_signed<BaseType>::value;                                                            \
        }                                                                                                      \
                                                                                                               \
        size_t EnumTraits<ENAME>::GetSizeBytes() const {                                                       \
            return sizeof(ENAME);                                                                              \
        }                                                                                                      \
                                                                                                               \
        bool EnumTraits<ENAME>::IsSerializable() const {                                                       \
            return IS_SERIALIZABLE;                                                                            \
        }                                                                                                      \
                                                                                                               \
        void EnumTraits<ENAME>::ForEach(void (*fn)(uint64_t, const char *, void *), void *fn_context) const {  \
            CONCAT3(InternalGet, ENAME, EnumName)({}, fn, fn_context);                                         \
        }                                                                                                      \
                                                                                                               \
        const EnumTraits<ENAME> EnumTraits<ENAME>::s_traits;                                                   \
                                                                                                               \
        EEND_EXTRA()

#define EEND() EEND__BODY()
#define EEND_SERIALIZABLE() EEND__BODY()

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

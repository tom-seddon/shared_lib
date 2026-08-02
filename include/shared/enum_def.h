#include "enums.h"
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

#ifdef _MSC_VER

#define EEND__EXTRA()
#define EEND__USED_RETAIN()

#else

#define EEND__EXTRA()
#define EEND__USED_RETAIN()

#endif

#define EBEGIN__BODY(TYPE)                                                                                       \
    const typename EnumTraits<ENAME>::GetNameFn EnumTraits<ENAME>::GET_NAME_FN = &CONCAT3(Get, ENAME, EnumName); \
                                                                                                                 \
    template <>                                                                                                  \
    const EnumTraitsBase *GetEnumTraits<ENAME>() {                                                               \
        return &EnumTraits<ENAME>::s_traits;                                                                     \
    }                                                                                                            \
                                                                                                                 \
    EBEGIN__INTERNAL_GET_PREFIX(TYPE)

#define EBEGIN__INTERNAL_GET_PREFIX(TYPE)                                               \
    static const char *CONCAT3(InternalGet, ENAME, EnumName)(TYPE value,                \
                                                             EnumTraitsBase * traits) { \
        static_assert(sizeof(TYPE) <= sizeof(uint64_t));                                \
                                                                                        \
        EnumValue *prev_value = nullptr;                                                \
                                                                                        \
        if (traits) {                                                                   \
            traits->MustBeUninitialized(); /* don't require debug.h */                  \
            goto fall_through;                                                          \
        }                                                                               \
                                                                                        \
        switch (value) {                                                                \
        fall_through:

#define EBEGIN() EBEGIN__BODY(int)
#define EBEGIN_DERIVED(BASE_NAME) EBEGIN__BODY(BASE_NAME)

#define EN__ENUM_VALUE(NAME, STR, ...)             \
    BEGIN_MACRO {                                  \
        static EnumValue s_value{__VA_ARGS__};     \
        s_value.traits = traits;                   \
        s_value.name = (STR);                      \
        s_value.ui_name = s_value.name;            \
        s_value.value = (uint64_t)(int64_t)(NAME); \
        if (!traits->first_value) {                \
            traits->first_value = &s_value;        \
        } else {                                   \
            prev_value->next = &s_value;           \
        }                                          \
        prev_value = &s_value;                     \
    }                                              \
    END_MACRO

#define EN__CASE(NAME, STR, ...) \
    EFALLTHROUGH;                \
    case (NAME):                 \
        if (!traits) {           \
            return (STR);        \
        }                        \
        EN__ENUM_VALUE((NAME), (STR)__VA_OPT__(, ) __VA_ARGS__);

#define EN(NAME) EN__CASE(NAME, #NAME)

#define EPN(NAME) EN__CASE(CONCAT3(ENAME, _, NAME), STRINGIZE(NAME));

#define EPN_BIT_FLAG(NAME, BIT) \
    EN__CASE(CONCAT3(ENAME, _, NAME), STRINGIZE(NAME), (BIT), 1)

#define EPN_BIT_FIELD(NAME, BIT, WIDTH) EN__ENUM_VALUE((((uint64_t)1 << (WIDTH)) - 1) << (BIT), #NAME, (BIT), (WIDTH));

#define EPN_BIT_FIELD_ENUM(NAME, BIT, WIDTH, ENAME2) EN__ENUM_VALUE((((uint64_t)1 << (WIDTH)) - 1) << (BIT), #NAME, (BIT), (WIDTH), &EnumTraits<ENAME2>::s_traits);

#define ENV(NAME, VALUE) EN(NAME)

#define EPNV(NAME, VALUE) EPN(NAME)

#define EQN(NAME)
#define EQNV(NAME, VALUE)
#define EQPN(NAME)
#define EQPNV(NAME, VALUE)

#define EMETA_WIP() traits->wip = true;
#define EMETA_SIZE_BITS(N) traits->size_bits = (N);

#define EUI_NAME(STR) prev_value->ui_name = (STR);

#define EEND__INTERNAL_GET_SUFFIX                                                    \
    EFALLTHROUGH;                                                                    \
    default:                                                                         \
        if (!traits) {                                                               \
            return "?" STRINGIZE(ENAME) "?";                                         \
        }                                                                            \
        }                                                                            \
                                                                                     \
        /* only gets here for one-time call for filling in the traits properties. */ \
        return nullptr;                                                              \
        }

#define EEND__BODY(SERIALIZABLE_HASH, EEND_FILE, EEND_LINE)                                                \
    EEND__INTERNAL_GET_SUFFIX                                                                              \
                                                                                                           \
    EPREFIX UNUSED const char *CONCAT3(Get, ENAME, EnumName)(typename EnumTraits<ENAME>::BaseType value) { \
        return CONCAT3(InternalGet, ENAME, EnumName)(value, nullptr);                                      \
    }                                                                                                      \
                                                                                                           \
    EnumTraits<ENAME>::EnumTraits() {                                                                      \
        /* get the field initialization out of the macro */                                                \
        this->Init1(STRINGIZE(ENAME),                                                                      \
                              std::is_signed<BaseType>::value,                                             \
                              sizeof(ENAME),                                                               \
                              SERIALIZABLE_HASH,                                                           \
                              EEND_LINE,                                                                   \
                              EEND_FILE);                                                                  \
                                                                                                           \
        /* do this as a 2nd step, so any of the properties can be overwritten. */                          \
        CONCAT3(InternalGet, ENAME, EnumName)({}, this);                                                   \
                                                                                                           \
        this->Init2();                                                                                     \
    }                                                                                                      \
                                                                                                           \
    EPREFIX const EnumTraitsBase *CONCAT3(Get, ENAME, EnumTraits)() {                                      \
        return &EnumTraits<ENAME>::s_traits;                                                               \
    }                                                                                                      \
                                                                                                           \
    EEND__EXTRA()

#define EEND() EEND__BODY(nullptr, __FILE__, __LINE__)
#define EEND_SERIALIZABLE(HASH) EEND__BODY(HASH, __FILE__, __LINE__)

/* This gets a bit freakish. */
#define NBEGIN_DERIVED(BASE_TYPE) NBEGIN__BODY(BASE_TYPE)
#define NBEGIN() NBEGIN__BODY(ENAME)

#define NBEGIN__BODY(BASE_TYPE)                 \
    typedef BASE_TYPE CONCAT2(ENAME, BaseType); \
                                                \
    EBEGIN__INTERNAL_GET_PREFIX(BASE_TYPE)

//EPREFIX const char *CONCAT3(Get, ENAME, EnumName)(BASE_TYPE value) { \
    //    switch (value) {                                                 \
    //    default:                                                         \
    //        return "?" STRINGIZE(NAME) "?";

//#define NN(NAME) \
//    case (NAME): \
//        return #NAME;

#define NN(NAME) EN__CASE((NAME), #NAME)

//#define NNS(NAME, STR) \
//    case (NAME):       \
//        return (STR);

#define NNS(NAME, STR) EN__CASE((NAME), (STR))

#define NEND()                                                                                 \
    EEND__INTERNAL_GET_SUFFIX                                                                  \
                                                                                               \
    struct CONCAT2(NEnumTraits, ENAME)                                                         \
        : public EnumTraitsBaseTyped<CONCAT2(ENAME, BaseType)> {                               \
        typedef BaseType EnumType;                                                             \
        /*typedef const char *(*GetNameFn)(BaseType);*/                                        \
        /*static const GetNameFn GET_NAME_FN;*/                                                \
        /* TODO: serializable should be an option... */                                        \
        static constexpr bool IS_SERIALIZABLE = false;                                         \
        CONCAT2(NEnumTraits, ENAME)() {                                                        \
            /* get the field initialization out of the macro */                                \
            this->Init1(STRINGIZE(ENAME),                                                      \
                                  std::is_signed<BaseType>::value,                             \
                                  sizeof(BaseType),                                            \
                                  nullptr,                                                     \
                                  __LINE__,                                                    \
                                  __FILE__);                                                   \
                                                                                               \
            /* do this as a 2nd step, so any of the properties can be overwritten. */          \
            CONCAT3(InternalGet, ENAME, EnumName)({}, this);                                   \
                                                                                               \
            this->Init2();                                                                     \
        }                                                                                      \
        static const CONCAT2(NEnumTraits, ENAME) s_traits;                                     \
    };                                                                                         \
                                                                                               \
    EEND__USED_RETAIN()                                                                        \
    inline const CONCAT2(NEnumTraits, ENAME) CONCAT2(NEnumTraits, ENAME)::s_traits;            \
                                                                                               \
    EPREFIX const char *UNUSED CONCAT3(Get, ENAME, EnumName)(CONCAT2(ENAME, BaseType) value) { \
        return CONCAT3(InternalGet, ENAME, EnumName)(value, nullptr);                          \
    }                                                                                          \
                                                                                               \
    EPREFIX const EnumTraitsBase *CONCAT3(Get, ENAME, EnumTraits)() {                          \
        return &CONCAT2(NEnumTraits, ENAME)::s_traits;                                         \
    }                                                                                          \
                                                                                               \
    EEND__EXTRA()

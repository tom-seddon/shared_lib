#include "enums.h"

#if EINTERNAL
#define EPREFIX static
#else
#define EPREFIX
#endif

#ifdef _MSC_VER

#define EEND__EXTRA()
#define EEND__USED_RETAIN()

#else

#define EEND__EXTRA()
#define EEND__USED_RETAIN() __attribute__((used, retain))

#endif

// The ENAME##BaseType thing is a bit ugly, but at this point the actual type
// isn't available. And when an ordinary enum, it can't be forward declared.
#define EBEGIN__BODY(COLON_BASE_TYPE, BASE_TYPE)                               \
    typedef BASE_TYPE CONCAT2(ENAME, BaseType);                                \
    EPREFIX const char *UNUSED CONCAT3(Get, ENAME, EnumName)(BASE_TYPE value); \
    enum ENAME COLON_BASE_TYPE {

#define EBEGIN() EBEGIN__BODY(, int)
#define EBEGIN_DERIVED(BASE_NAME) EBEGIN__BODY( : BASE_NAME, BASE_NAME)

#define EN(NAME) NAME,
#define ENV(NAME, VALUE) NAME = (VALUE),
#define EPN(NAME) EN(CONCAT3(ENAME, _, NAME))
#define EPNV(NAME, VALUE) ENV(CONCAT3(ENAME, _, NAME), VALUE)

#define EPN_BIT_FLAG(NAME, BIT) \
    EQPNV(NAME, 1 << (BIT))     \
    EPN_BIT_FIELD(NAME, (BIT), 1)

#define EPN_BIT_FIELD(NAME, BIT, WIDTH) \
    EQPNV(CONCAT2(NAME, Shift), (BIT))  \
    EQPNV(CONCAT2(NAME, Mask), (1u << (WIDTH)) - 1)

#define EQN(NAME) EN(NAME)
#define EQNV(NAME, VALUE) ENV(NAME, VALUE)
#define EQPN(NAME) EPN(NAME)
#define EQPNV(NAME, VALUE) EPNV(NAME, VALUE)

#define EMETA_SIZE_BITS(N)

#define EEND__BODY(SERIALIZABLE_HASH, IS_SERIALIZABLE_CONSTEXPR)             \
    }                                                                        \
    ;                                                                        \
                                                                             \
    template <>                                                              \
    struct EnumTraits<ENAME> : public EnumTraitsBase {                       \
        typedef ENAME EnumType;                                              \
        typedef CONCAT2(ENAME, BaseType) BaseType;                           \
        typedef const char *(*GetNameFn)(BaseType);                          \
        static const GetNameFn GET_NAME_FN;                                  \
        static constexpr bool IS_SERIALIZABLE = (IS_SERIALIZABLE_CONSTEXPR); \
        EnumTraits();                                                        \
        static const EnumTraits<ENAME> s_traits;                             \
    };                                                                       \
                                                                             \
    EEND__USED_RETAIN()                                                      \
    inline const EnumTraits<ENAME> EnumTraits<ENAME>::s_traits;              \
                                                                             \
    EEND__EXTRA()

#define EEND_SERIALIZABLE(HASH) EEND__BODY(HASH, true)
#define EEND() EEND__BODY(nullptr, false)

//#define EOVERLOAD() const char *GetEnumValueName(ENAME value);

#define NBEGIN(NAME) EPREFIX const char *UNUSED CONCAT3(Get, NAME, EnumName)(ENAME value);
#define NEND()
#define NN(NAME)
//#define NOVERLOAD() const char *GetEnumValueName(ENAME value);

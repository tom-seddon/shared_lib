#ifndef HEADER_8427D66EED644D3B9EE1A77C3A077053 // -*- mode:c++ -*-
#define HEADER_8427D66EED644D3B9EE1A77C3A077053

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This header behaves differently when nlohmann/json.hpp was previously
// included. When including nlohmann/json.hpp, it should be included before
// including this file.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
struct EnumBaseType;

template <class T>
struct EnumTraits;

class EnumTraitsBase;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
const EnumTraitsBase *GetEnumTraits();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These serve as a kind of markup for serialization. Enough overloaded
// operators and whatnot are provided to ensure that for most code it'll compile
// just as well (though perhaps run less efficiently...) with Enum<T> as with T.
//
// The overloaded operator& is handy for non-serialization code, but the
// serialization code has to use non-const references rather than pointers.

template <class T>
struct Enum {
    typedef T ValueType;
    ValueType value{};

    Enum() = default;
    Enum(ValueType value_)
        : value(value_) {
    }
    operator ValueType() const {
        return this->value;
    }
    ValueType *operator&() {
        return &this->value;
    }
    const ValueType *operator&() const {
        return &this->value;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
struct EnumFlags {
    typedef typename EnumTraits<T>::BaseType ValueType;
    ValueType value{};

    EnumFlags() = default;
    EnumFlags(ValueType value_)
        : value(value_) {
    }
    operator ValueType() const {
        return this->value;
    }
    ValueType *operator&() {
        return &this->value;
    }
    const ValueType *operator&() const {
        return &this->value;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
inline const char *GetEnumName(T value) {
    return (*EnumTraits<T>::GET_NAME_FN)(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class EnumTraitsBase;

struct EnumValue {
    const EnumValue *next = nullptr;
    const EnumTraitsBase *traits = nullptr;

    // Name of the value, derived from the name it has in the sourco code.
    const char *name = nullptr;

    // Human-facing name for the value, if specified. By default, this is the
    // same as this->name, but it can be tweaked if making the name palatable to
    // the compiler made too much of a mess.
    //
    // As with name, this is intended for use as an identifier kind of thing,
    // for typing in on the command line and that sort of thing.
    const char *ui_name = nullptr;

    // If the enum type is signed, the value will have been sign-extended from
    // its original width; if unsigned, zero-extended. Check IsSigned() and
    // GetSizeBytes() for the owning traits for more info.
    //
    // The value of a bit field is the mask for its bits.
    uint64_t value = 0;

    int8_t bit_shift = -1;                    //>=0 if a bitfield
    uint8_t bit_width = 0;                    //>0 if a bitfield
    const EnumTraitsBase *bit_enum = nullptr; //if an enum

    EnumValue() = default;
    EnumValue(int8_t bit_shift, uint8_t bit_width, const EnumTraitsBase *bit_enum = nullptr);
};

class EnumTraitsBase {
  public:
    const EnumTraitsBase *next = nullptr;
    const char *name = nullptr;
    bool is_signed = false;
    bool is_bitfield = false;
    size_t size_bytes = 0;
    size_t size_bits = 0;
    int width_xdigits = 0; //for use with printf
    const char *serializable_hash = nullptr;
    int eend_line = -1;
    const char *eend_file = nullptr;
    const EnumValue *first_value = nullptr;
    bool wip = false;

    EnumTraitsBase();
    virtual ~EnumTraitsBase() = default;

    EnumTraitsBase(const EnumTraitsBase &) = delete;
    EnumTraitsBase &operator=(const EnumTraitsBase &) = delete;
    EnumTraitsBase(EnumTraitsBase &&) = delete;
    EnumTraitsBase &operator=(EnumTraitsBase &&) = delete;

    static const EnumTraitsBase *GetFirst();

    void MustBeUninitialized();

    // slightly ropey helpers for non-templated code.
    virtual uint64_t GetValue(const void *ptr) const = 0;
    virtual void SetValue(void *ptr, uint64_t value) const = 0;

    const EnumValue *FindEnumValue(const void *ptr) const;

  protected:
    void Init1(const char *name, bool is_signed, size_t size_bits, const char *serializable_hash, int eend_line, const char *eend_file);
    void Init2();

  private:
};

template <class T>
class EnumTraitsBaseTyped : public EnumTraitsBase {
  public:
    typedef T BaseType;

    uint64_t GetValue(const void *ptr) const override {
        return (uint64_t)(int64_t)*(BaseType *)ptr;
    }

    void SetValue(void *ptr, uint64_t value) const override {
        *(BaseType *)ptr = (BaseType)value;
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE

template <class T>
void to_json(nlohmann::json &j, const Enum<T> &value) {
    static_assert(EnumTraits<T>::IS_SERIALIZABLE);
    j = (*EnumTraits<T>::GET_NAME_FN)(value.value);
}

template <class T>
void from_json(const nlohmann::json &j, Enum<T> &value) {
    static_assert(EnumTraits<T>::IS_SERIALIZABLE);
    std::string str = j.get<std::string>();

    typename EnumTraits<T>::BaseType i = 0;
    for (;;) {
        const char *name = (*EnumTraits<T>::GET_NAME_FN)(i);
        if (name[0] == '?') {
            // Reached end of list.
            break;
        }

        if (str == name) {
            value.value = static_cast<T>(i);
            break;
        }

        ++i;
    }
}

template <class T>
void from_json(const nlohmann::json &j, EnumFlags<T> &value) {
    static_assert(EnumTraits<T>::IS_SERIALIZABLE);
    if (j.is_array()) {
        value.value = 0;

        for (size_t i = 0; i < j.size(); ++i) {
            if (j[i].is_string()) {
                const std::string &j_name = j[i].get<std::string>();

                for (typename std::make_unsigned<typename EnumTraits<T>::BaseType>::type mask = 1; mask != 0; mask <<= 1) {
                    const char *name = (*EnumTraits<T>::GET_NAME_FN)(static_cast<typename EnumTraits<T>::BaseType>(mask));
                    if (name[0] == '?') {
                        continue;
                    }

                    if (j_name == name) {
                        value.value = static_cast<typename EnumTraits<T>::BaseType>(value.value | mask);
                        break;
                    }
                }
            }
        }
    }
}

template <class T>
void to_json(nlohmann::json &j, const EnumFlags<T> &value) {
    static_assert(EnumTraits<T>::IS_SERIALIZABLE);
    j = nlohmann::json::array_t{};
    for (typename std::make_unsigned<typename EnumTraits<T>::BaseType>::type mask = 1; mask != 0; mask <<= 1) {
        const char *name = (*EnumTraits<T>::GET_NAME_FN)(static_cast<typename EnumTraits<T>::BaseType>(mask));
        if (name[0] == '?') {
            continue;
        }

        if (!(value.value & static_cast<typename EnumTraits<T>::BaseType>(mask))) {
            continue;
        }

        j.push_back(name);
    }
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void EnsureEnumsInitialised();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

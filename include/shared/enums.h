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

class EnumTraitsBase {
  public:
    EnumTraitsBase();
    virtual ~EnumTraitsBase() = default;

    EnumTraitsBase(const EnumTraitsBase &) = delete;
    EnumTraitsBase &operator=(const EnumTraitsBase &) = delete;
    EnumTraitsBase(EnumTraitsBase &&) = delete;
    EnumTraitsBase &operator=(EnumTraitsBase &&) = delete;

    virtual const char *GetEnumName() const = 0;
    virtual bool IsSigned() const = 0;
    virtual size_t GetSizeBytes() const = 0;

    virtual const char *GetSerializableHash() const = 0;
    virtual int64_t GetEENDLine() const = 0;
    virtual const char *GetEENDFile() const = 0;

    // If signed, the value will have been sign-extended from its original width; if unsigned, zero-extended. Check IsSigned() and GetSizeBytes() for more info.
    virtual void ForEach(void (*fn)(uint64_t value, const char *name, const EnumTraitsBase *traits, void *context), void *fn_context) const = 0;

    static const EnumTraitsBase *GetFirst();
    const EnumTraitsBase *GetNext() const;

  protected:
  private:
    const EnumTraitsBase *m_next_enum_traits = nullptr;
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

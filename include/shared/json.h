#ifndef HEADER_D12F1E923F7147199D459D8DEBE1B1B4 // -*- mode:c++ -*-
#define HEADER_D12F1E923F7147199D459D8DEBE1B1B4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This header behaves differently when nlohmann/json.hpp was previously
// included. When including nlohmann/json.hpp, it should be included before
// including this file.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// If json.hpp has been included:
//
// - JSON_SERIALIZE expands to NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
//
// - JSON is a struct derived from nlohmann::json
//
// Otherwise:
//
// - JSON_SERIALIZE expands to a no-op static_assert
//
// - JSON is a forward-declared struct

#ifdef NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE

#define JSON_SERIALIZE NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT

struct JSON : private nlohmann::json {
    JSON() = default;

    explicit JSON(const nlohmann::json &j)
        : nlohmann::json(j) {
    }

    JSON(nlohmann::json &&j)
        : nlohmann::json(std::move(j)) {
    }

    // couldn't figure out how to make a conversion operator out of this one.
    const nlohmann::json &AsNLohmannJSON() const {
        return *this;
    }

    template <class T>
    void Load(T *value) const {
        try {
            *value = this->template get<T>();
        } catch (nlohmann::json::exception &) {
            // ...
        }
    }

    template <class T>
    void Save(const T &value) {
        *static_cast<nlohmann::json *>(this) = value;
    }
};

#else

#define JSON_SERIALIZE(...) static_assert(true)

struct JSON;

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

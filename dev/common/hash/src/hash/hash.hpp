#pragma once

#include "hash_utils.hpp"
#include "unique_id.hpp"
#include "hashing_algorithms.hpp"


namespace hashing {


template<hash_algorithm HashAlgorithm, typename T>
void add_to_hash(HashAlgorithm& h, const T& t) 
requires requires { h(t); } {
    h(t);
}

template<hash_algorithm HashAlgorithm, std::floating_point F>
void add_to_hash(HashAlgorithm& h, F f) {
    if(f == 0.0f) {
        f = 0.0f;
    }
    h(&f, sizeof(f));
}

namespace detail {

static constexpr bool AllowForStdHash = true;

template<hash_algorithm HashAlgorithm, typename T>
constexpr void apply_hash(HashAlgorithm& h, const T& t) {

    if constexpr( requires{ add_to_hash(h, t); } ) {
        add_to_hash(h, t);
    }
    else if constexpr( requires{ h(t); } ) {
        h(t);
    }
    else if constexpr( requires{ hash_decompose(t); }) {
        std::apply([&](auto&&... args) { (add_to_hash(h, args), ...); }, hash_decompose(t));
    }
    else if constexpr( AllowForStdHash && requires{ std::hash<T>{}(t); }) {
        h(std::hash<T>{}(t));
    }
    else {
        static_assert(false, "Please provide an 'add_to_hash' or 'hash_decompose' overload for this type");
    }
}

} // namespace detail

template<hash_algorithm HashAlgorithm = fnv1a_64, bool AppendTypeHashCode = true>
class hash {
public:
    using result_type = typename HashAlgorithm::result_type;

    template<typename T>
    constexpr result_type operator()(const T& t) const noexcept {
        HashAlgorithm h;
        detail::apply_hash(h, t);

        if constexpr(AppendTypeHashCode) {
            h(type_hash_code<T>);
        }

        return static_cast<result_type>(h);
    }
};


} // namespace hashing

#pragma once

#include <type_traits>
#include <ranges>

// #include "ints.hpp"
// todo remove those
#include "../../../../base/src/base/ints.hpp"

#include "hash_utils.hpp"


namespace hashing {


class fnv1a_32 : public call_overloads {
    friend call_overloads;

    static constexpr u32 offset_basis = 2166136261u;
    static constexpr u32 FNV_prime = (1u << 24) + (1u << 8) + 0x93u;
    u32 state = offset_basis;

    constexpr void update_hash(const volatile void* data, usize len) noexcept {
        for (usize i = 0; i < len; ++i) {
            state ^= static_cast<const volatile unsigned char*>(data)[i];
            state *= FNV_prime;
        }
    }

public:
    using result_type = u32;

    constexpr fnv1a_32() = default;
    constexpr fnv1a_32(u32 state) : state(state) {}

    constexpr explicit operator result_type() noexcept {
        return static_cast<result_type>(state);
    }
};

class fnv1a_64 : public call_overloads {
    friend call_overloads;

    static constexpr u64 offset_basis = 14695981039346656037ull;
    static constexpr u64 FNV_prime = (1ull << 40) + (1ull << 8) + 0xb3ull;
    u64 state = offset_basis;

    constexpr void update_hash(const volatile void* data, usize len) noexcept {
        for (usize i = 0; i < len; ++i) {
            state ^= static_cast<const volatile unsigned char*>(data)[i];
            state *= FNV_prime;
        }
    }

public:
    using result_type = u64;

    constexpr fnv1a_64() = default;
    constexpr fnv1a_64(u64 state) : state(state) {}

    constexpr explicit operator result_type() noexcept {
        return static_cast<result_type>(state);
    }
};


} // namespace hash

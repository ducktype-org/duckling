#pragma once

#include<cstddef>
#include<cstdint>
#include<version>

#define SER_VERSION_MAJOR 0
#define SER_VERSION_MINOR 1
#define SER_VERSION_PATCH 0
#define SER_VERSION (SER_VERSION_MAJOR * 10000 + SER_VERSION_MINOR * 100 + SER_VERSION_PATCH)

#define SER_MIN_CPLUSPLUS 202100L

#if defined(_MSC_VER)
#  if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
#    error "ser: /Zc:preprocessor is required - without it __VA_OPT__ does not work"
#  endif
#  if __cplusplus < SER_MIN_CPLUSPLUS
#    error "ser: /std:c++latest and /Zc:__cplusplus are required"
#  endif
#else
#  if __cplusplus < SER_MIN_CPLUSPLUS
#    error "ser: C++23 is required (-std=c++23 or -std=c++2b)"
#  endif
#endif

#if defined(__cpp_impl_reflection) && defined(__cpp_expansion_statements)
#  define SER_HAS_REFLECTION 1
#else
#  define SER_HAS_REFLECTION 0
#endif

#if defined(__cpp_structured_bindings) && __cpp_structured_bindings >= 202411L
#  define SER_HAS_SB_PACKS 1
#else
#  define SER_HAS_SB_PACKS 0
#endif

#if defined(__cpp_lib_start_lifetime_as)
#  define SER_HAS_START_LIFETIME_AS 1
#else
#  define SER_HAS_START_LIFETIME_AS 0
#endif

#if defined(__cpp_lib_saturation_arithmetic)
#  define SER_HAS_SATURATING 1
#else
#  define SER_HAS_SATURATING 0
#endif

#if defined(__cpp_lib_inplace_vector)
#  define SER_HAS_INPLACE_VECTOR 1
#else
#  define SER_HAS_INPLACE_VECTOR 0
#endif

#if defined(__cpp_lib_is_implicit_lifetime)
#  define SER_HAS_IS_IMPLICIT_LIFETIME 1
#else
#  define SER_HAS_IS_IMPLICIT_LIFETIME 0
#endif

#if defined(__cpp_contracts)
#  define SER_HAS_CONTRACTS 1
#else
#  define SER_HAS_CONTRACTS 0
#endif

#if defined(__SANITIZE_ADDRESS__)
#  define SER_HAS_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define SER_HAS_ASAN 1
#  else
#    define SER_HAS_ASAN 0
#  endif
#else
#  define SER_HAS_ASAN 0
#endif

#if defined(_MSC_VER)
#  define SER_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#  define SER_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#if defined(__has_builtin)
#  if __has_builtin(__builtin_bit_cast)
#    define SER_BIT_CAST(T, x) __builtin_bit_cast(T, x)
#  endif
#elif defined(_MSC_VER) && _MSC_VER >= 1927
#  define SER_BIT_CAST(T, x) __builtin_bit_cast(T, x)
#endif
#ifndef SER_BIT_CAST
#  define SER_BIT_CAST(T, x) ::std::bit_cast<T>(x)
#endif


#if defined(_MSC_VER)
#  define SER_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#  define SER_FORCEINLINE inline __attribute__((always_inline))
#else
#  define SER_FORCEINLINE inline
#endif

#ifndef SER_DEBUG_POISON
#  ifdef NDEBUG
#    define SER_DEBUG_POISON 0
#  elif SER_HAS_ASAN
#    define SER_DEBUG_POISON 2
#  else
#    define SER_DEBUG_POISON 1
#  endif
#endif

#ifndef SER_HASH_MAP
#  define SER_HASH_MAP        ::std::unordered_map
#  define SER_HASH_MAP_HEADER <unordered_map>
#endif
#ifndef SER_HASH_MAP_HEADER
#  error "ser: when defining SER_HASH_MAP you must also define SER_HASH_MAP_HEADER"
#endif

namespace ser {
    struct config_global {
        // use u64 as size_type to avoid silent overflows from size_t conversions
        using size_type = ::std::uint64_t;

        // Max depth when serializing/deserializing nested structures.
        // This is to avoid stack overflows and excessive recursion.
        static constexpr ::std::size_t max_depth = 256;

        static constexpr ::std::size_t max_container_elements = ::std::size_t{1} << 28;

        // Whether to align flat arrays (like std::array) to their element type's alignment.
        // This ensures the array elements are properly aligned in memory, which is
        // required for zero-copy reads.
        static constexpr bool align_flat_arrays = true;
    };

} // namespace ser

#pragma once

// ── min_wire_size_v<T> ────────────────────────────────────────────────────────
// The fewest bytes T can possibly occupy in a stream. It exists for one job: a length
// prefix arrives before any of the elements it counts, and `n` elements cannot be there
// unless `n * min_wire_size_v<E>` bytes are. Without that arithmetic a corrupt prefix is
// an allocation, and the first thing an attacker reaches for is a length field.
//
// A LOWER bound, never an estimate. Anything this cannot measure answers 1 - one byte,
// because a type that occupies nothing cannot be told apart from the next one - and a
// bound that is too small only weakens the check, while one that is too large would
// reject a stream that is perfectly valid.
//
// It is a class template rather than a function so that the std adapters can specialize
// it for their own containers: a vector's minimum is its prefix, whatever the element is.

#include <ser/config.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/meta.hpp>

#include <cstddef>
#include <type_traits>

namespace ser {

    template <class T>
    struct min_wire_size;

    template <class T>
    inline constexpr ::std::size_t min_wire_size_v = min_wire_size<::std::remove_cv_t<T>>::value;

    namespace detail {

        template <class... Fs>
        [[nodiscard]] consteval ::std::size_t sum_min_wire(type_list<Fs...>) {
            return (::std::size_t{0} + ... + min_wire_size_v<Fs>);
        }

        template <class T>
        [[nodiscard]] consteval ::std::size_t min_wire_compute() {
            // bool is one byte on the wire whatever sizeof(bool) is on this platform -
            // the object representation never reaches the stream, and block F hashes the
            // wire size for exactly this reason.
            if constexpr (::std::is_same_v<T, bool>)
                return 1;
            else if constexpr (builtin::scalar_like<T> || builtin::enum_like<T>)
                return sizeof(T);
            else if constexpr (builtin::array_like<T>)
                return builtin::array_length_v<T> * min_wire_size_v<builtin::array_element_t<T>>;
            else if constexpr (::std::is_empty_v<T>)
                return 0;                       // truthful: an empty type writes nothing
            else if constexpr (can_enumerate_members_v<T>)
                return sum_min_wire(field_types_t<T>{});
            else
                return 1;                       // a hook, a container, anything unmeasured
        }

    } // namespace detail

    template <class T>
    struct min_wire_size {
        static constexpr ::std::size_t value = detail::min_wire_compute<T>();
    };

} // namespace ser

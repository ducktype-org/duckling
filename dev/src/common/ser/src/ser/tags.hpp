#pragma once
 
namespace ser {

    // The type an ADL ser_make dispatches on. A function cannot be overloaded on its
    // return type, so `ser_make(ar)` alone could never say WHICH type to build - the tag
    // is the argument that carries it, and it also drags namespace ser into the
    // associated set so the barrier in detail/adl.hpp finds the hook.
    template <class T>
    struct tag { using type = T; };

    struct raw_init_t { explicit raw_init_t() = default; };
    inline constexpr raw_init_t raw_init{};
    
    struct in_place_t { explicit in_place_t() = default; };
    inline constexpr in_place_t in_place{};
    
    struct view_t { explicit view_t() = default; };
    
    struct sized_t { explicit sized_t() = default; };

    struct unsized_t { explicit unsized_t() = default; };
    inline constexpr unsized_t unsized_tag{};

    struct bytes_t { explicit bytes_t() = default; };
    inline constexpr bytes_t bytes_tag{};
 
}  // namespace ser

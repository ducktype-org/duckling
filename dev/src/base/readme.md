# Base

Base is a top-level module dedicated for standard-library-like implementations. It exports some new functionalities as well as functionalities that change or expand `std::`. It should be preferred over `std::`.

- anycast.hpp
- constexpr_cat.hpp
- convert.hpp
- defer.hpp
- argument_splitter.hpp
- [exceptions.hpp](@ref base/exceptions.hpp)
- flag.hpp
- init_guard.hpp
- ints.hpp
- floats.hpp
- maps.hpp
- perfect_hash.hpp
- raw_view.hpp
- smart_pointers.hpp
- stable_container.hpp
- stable_hashmap.hpp
- str_utils.hpp
- string_id.hpp
- stringifyable_enum.hpp
- strongly_typed_id.hpp
- strongly_typed_int.hpp
- type_traits.hpp
- [variant.hpp](@ref base/variant.hpp)

Macros that are used as helpers for other macros are defined in `macro` folder
- utils.hpp - common simple utils macros like EXPAND, STRIGIFY_2, COMMA, IF
- for_each.hpp
- diagnostics.hpp

@TODO Generate list of files automatically, currently it is hardcoded.

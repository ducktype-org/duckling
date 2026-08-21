#pragma once

#include <ser/access.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/pointer_deny.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

#include <ser/detail/adl.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser::detail {

    // ── what the walk refuses to take apart ───────────────────────────────────
    // Two field shapes that every other guard in this file is structurally unable to see,
    // because both are properties of the DECLARATION and an expression never carries
    // either. field_decls_t is the one thing that still knows - see the note on
    // member_type_t in describe_bind.hpp.
    //
    // This is asked only where the walk is actually chosen, so a type with a hook never
    // pays for it and never has its fields judged.
    template <class... Ds>
    consteval void check_field_decls(type_list<Ds...>) {
        static_assert(!(::std::is_reference_v<typename Ds::type> || ... || false),
            "ser: cannot serialize a REFERENCE field. Reading has to overwrite the object "
            "and a reference can never be rebound, so there is no honest thing to do with "
            "one: built out of fresh values it would point at a temporary that dies "
            "immediately, and filled in place it would silently overwrite whatever it "
            "already pointed at. Hold the value itself, or list the fields with "
            "SER_DESCRIBE(a, b) and leave the reference out.");
    }

    template <class T>
    consteval void check_walkable_fields() {
        check_field_decls(field_decls_t<T>{});
    }

    // -- fill, or build? -------------------------------------------------------
    // The walk never assigns the object. It assigns the LEAVES: dispatch_read of a scalar
    // field writes bytes into that field, in place, through a reference. So what it needs
    // is per-field and not per-object - every field reachable by a writable reference,
    // which rules out a const field and a bit-field. Whole-object assignability is a
    // different question and is asked in exactly one place, step 9, which really does
    // `x = dispatch_make<U>(ar)`.
    template <class T, class Ar> consteval bool readable_in_place();
    template <class T, class Ar> consteval bool walk_can_fill();

    template <class Ar, class... Ds>
    consteval bool walk_can_fill_fields(type_list<Ds...>) {
        return (!::std::is_const_v<typename Ds::type> && ... && true)
            && (Ds::bindable && ... && true)
            && (readable_in_place<::std::remove_cvref_t<typename Ds::type>, Ar>() && ... && true);
    }

    template <class T, class Ar>
    consteval bool walk_can_fill() {
        if constexpr (!can_enumerate_members_v<T>) return false;     // the walk is not the way
        else return walk_can_fill_fields<Ar>(field_decls_t<T>{});
    }

    template <class T, class Ar>
    inline constexpr bool walk_can_fill_v = walk_can_fill<T, Ar>();

    // "Can dispatch_read put a value into an object of this type that already exists?"
    // An if-constexpr chain rather than a conjunction on purpose: a type with a hook must
    // not have its fields inspected at all, because a hook is allowed to be the only thing
    // that understands them (invariant 7).
    template <class T, class Ar>
    consteval bool readable_in_place() {
        if constexpr (::std::is_move_assignable_v<T>)   return true;
        else if constexpr (has_any_read_hook_v<T, Ar>)  return true;   // the hook does it
        else if constexpr (builtin::denied_v<T>)        return true;   // refused elsewhere
        else if constexpr (builtin::scalar_like<T>)     return true;
        else if constexpr (builtin::enum_like<T>)        return true;
        else if constexpr (builtin::array_like<T>)       return true;
        else if constexpr (!can_enumerate_members_v<T>) return true;   // not this trait's call
        else return walk_can_fill<T, Ar>();                             // step 8, or nothing
    }

    // Step 9's question: the object exists and the walk cannot write into it, so build a
    // fresh one and assign. A type that offers only ser_make is the other caller.
    template <class T, class Ar>
    consteval bool read_by_building() {
        if constexpr (has_any_make_hook_v<T, Ar>)       return ::std::is_move_assignable_v<T>;
        else if constexpr (!can_enumerate_members_v<T>) return false;
        else if constexpr (walk_can_fill<T, Ar>())      return false;   // step 8 has it
        else return ::std::is_move_assignable_v<T>;
    }

    template <class T, class Ar>
    inline constexpr bool read_by_building_v = read_by_building<T, Ar>();

    // Step 4's question. Anything a hook or a builtin rule reads is left alone: the walk
    // is not what will read it, so its fields are none of this trait's business.
    template <class T, class Ar>
    consteval bool must_be_built() {
        if constexpr (has_any_read_hook_v<T, Ar>)       return false;
        else if constexpr (builtin::denied_v<T>)        return false;
        else if constexpr (builtin::scalar_like<T>)     return false;
        else if constexpr (builtin::enum_like<T>)        return false;
        else if constexpr (builtin::array_like<T>)       return false;
        else if constexpr (!can_enumerate_members_v<T>) return false;
        else return !walk_can_fill<T, Ar>();
    }

    template <class T, class Ar>
    inline constexpr bool must_be_built_v = must_be_built<T, Ar>();

    // ── depth guard ───────────────────────────────────────────────────────────
    // Data-dependent recursion (struct Tree { std::vector<Tree> kids; }) is an attack
    // vector on read and a stack overflow on write.
    template <class Ar>
    class depth_guard {
        Ar&  ar;
        bool held;
    public:
        explicit constexpr depth_guard(Ar& a) noexcept : ar(a), held(a.push_depth()) {}
        constexpr ~depth_guard() { if (held) ar.pop_depth(); }

        depth_guard(const depth_guard&)            = delete;
        depth_guard& operator=(const depth_guard&) = delete;

        [[nodiscard]] constexpr bool entered() const noexcept { return held; }
    };

    // ── the three dispatch contexts ───────────────────────────────────────────
    //
    //   dispatch_write   object exists, produce bytes
    //   dispatch_read    object exists, fill it from bytes
    //   dispatch_make    no object yet, build and return one
    //
    // The order of the ladder is the whole design. A user hook outranks a builtin rule,
    // a builtin rule outranks guessing, and within a level the asymmetric form outranks
    // the symmetric one. Levels run trait, then in-class, then ADL: ser::serializer<T>
    // has to win, because it is the only one available for a type you cannot edit.
    //
    // Two rules that look like details and are not:
    //   - a type with a hook is NEVER decomposed, whatever it otherwise looks like. This
    //     is what stops a container-shaped type with ser_visit from being walked as a
    //     container, and (M2) a remapped handle from being copied as bytes.
    //   - the refusals sit at the top, above every hook. A pointer with a serializer is
    //     still a pointer.

    template <class T, writer Ar>
    constexpr errc dispatch_write(Ar& ar, const T& x) {
        using U = ::std::remove_cv_t<T>;
        check_hooks<U, Ar>();

        depth_guard g{ar};
        if (!g.entered()) return errc::depth_exceeded;

        if constexpr (builtin::denied_v<U>) { builtin::deny<U>(); return errc::invalid_value; }

        else if constexpr (access::has_trait_write_v<U, Ar>)                                // 1
            return serializer<U>::write(ar, x);
        else if constexpr (access::has_trait_visit_v<const U, Ar>)                          // 2
            return serializer<U>::visit(ar, x);
        else if constexpr (access::has_member_write_v<U, Ar>)                               // 3
            return access::call_write<U>(ar, x);
        else if constexpr (access::has_member_visit_v<const U, Ar>)                         // 4
            return access::call_visit(ar, x);
        else if constexpr (has_adl_write_v<U, Ar>)                                          // 5
            return adl_barrier::call_write(ar, x);
        else if constexpr (has_adl_visit_v<const U, Ar>)                                    // 6
            return adl_barrier::call_visit(ar, x);

        else if constexpr (builtin::scalar_like<U>) return builtin::write_scalar<U>(ar, x); // 7
        else if constexpr (builtin::enum_like<U>)   return builtin::write_enum<U>  (ar, x);
        else if constexpr (builtin::array_like<U>)  return builtin::write_array<U> (ar, x);

        // Step 8. No hook, no builtin rule: walk the fields. It is last among the paths
        // that DO something, so a hook always outranks it - which is what keeps a
        // container-shaped type with ser_visit from being taken apart field by field.
        else if constexpr (can_enumerate_members_v<U>) {                                // 8
            check_walkable_fields<U>();
            return visit_members(x, [&ar](const auto&... fields) { return ar(fields...); });
        }

        // check_hooks has already reported the hook this archive cannot call; the
        // generic message below would only point away from it.
        else if constexpr (::std::is_aggregate_v<U> && !can_enumerate_members_v<U>)
            static_assert(dependent_false<T>,
                "ser: this type is an aggregate whose field count cannot be worked out. "
                "Two things cause it. A C ARRAY field elides braces, so counting sees its "
                "elements rather than the field. A BASE CLASS is an element of aggregate "
                "initialization but not of a structured binding, so counting sees one "
                "field too many. Declare the count in the class:\n"
                "  using ser_members = ser::members<N>;\n"
                "That settles the array, and a base with no data members of its own. A base "
                "that HAS data members cannot be decomposed at all - give the type a "
                "ser_visit hook.");

        else if constexpr (nongeneric_any_hook_v<U, Ar>) return errc::invalid_value;
        else if constexpr (builtin::looks_pointer_like<U>()) {                              // 9
            builtin::deny_pointer_like<U>();
            return errc::invalid_value;
        }
        else static_assert(dependent_false<T>,
            "ser: no way to serialize this type.\n"
            "  Aggregate with public fields?  nothing needed\n"
            "  Private fields?                using ser_members = ser::members<N>; + SER_FRIEND\n"
            "  Has a constructor?             SER_MAKE_FROM(Type, a, b, c);\n"
            "  Not your type?                 specialize ser::serializer<T>");
    }

    template <class T, reader Ar>
    constexpr errc dispatch_read(Ar& ar, T& x) {
        using U = ::std::remove_cv_t<T>;
        // Reachable from the member walk (block C) even though ser::in::operator() has
        // its own requires clause: a const FIELD gets here with the enclosing object
        // perfectly non-const.
        static_assert(!::std::is_const_v<T>,
            "ser: cannot read into a const object - reading overwrites it.\n"
            "  A whole object?  auto r = ser::read<T>(bytes);  and bind r to const\n"
            "  A const field?   SER_MAKE_FROM(Type, a, b, c); - a const field must be\n"
            "                   initialized at construction, it can never be assigned");

        check_hooks<U, Ar>();

        depth_guard g{ar};
        if (!g.entered()) return errc::depth_exceeded;

        if constexpr (builtin::denied_v<U>) { builtin::deny<U>(); return errc::invalid_value; }

        else if constexpr (access::has_trait_read_v<U, Ar>)                                 // 1
            return serializer<U>::read(ar, x);
        else if constexpr (access::has_trait_visit_v<U, Ar>)                                // 2
            return serializer<U>::visit(ar, x);
        else if constexpr (access::has_member_read_v<U, Ar>)                                // 3
            return access::call_read<U>(ar, x);
        else if constexpr (access::has_member_visit_v<U, Ar>)                               // 4
            return access::call_visit(ar, x);
        else if constexpr (has_adl_read_v<U, Ar>)                                           // 5
            return adl_barrier::call_read(ar, x);
        else if constexpr (has_adl_visit_v<U, Ar>)                                          // 6
            return adl_barrier::call_visit(ar, x);

        else if constexpr (builtin::scalar_like<U>) return builtin::read_scalar<U>(ar, x);  // 7
        else if constexpr (builtin::enum_like<U>)   return builtin::read_enum<U>  (ar, x);
        else if constexpr (builtin::array_like<U>)  return builtin::read_array<U> (ar, x);

        // Step 8. The walk must not take a type whose author wrote ser_make
        else if constexpr (!has_any_make_hook_v<U, Ar> && walk_can_fill_v<U, Ar>) {
            check_walkable_fields<U>();
            return visit_members(x, [&ar](auto&... fields) { return ar(fields...); });
        }

        // Step 9. Two kinds of type arrive here for the same reason: the object already
        // exists and cannot be written into field by field, so the only way left is to
        // build a fresh one and assign it. One is a type that offers only ser_make. The
        // other is a type the walk cannot fill - a BIT-FIELD field is that case, since
        // braces can initialize one and no reference can be bound to it. Either way it
        // costs one assignment, which is why a type that can offer ser_read should.
        else if constexpr (read_by_building_v<U, Ar>) {
            // dispatch_make signals by throwing; this context returns a code. The boundary
            // between the two conventions is here and nowhere else.
            try {
                x = dispatch_make<U>(ar);
                return errc::ok;
            } catch (const exception& e) {
                return e.code();
            }
        }

        else if constexpr (has_any_make_hook_v<U, Ar>) static_assert(dependent_false<T>,        // 10
            "ser: this type has ser_make but no ser_read, and it is not move-assignable, "
            "so an existing object cannot be filled in. Add ser_read(ar, x), or make the "
            "type move-assignable.");

        // The same dead end reached from the other side: the walk cannot write into this
        // type's fields and it cannot be built-and-assigned either. A CONST field is what
        // produces that pair, because const is exactly what makes a type non-assignable.
        // Such a type is still readable on its own - ser::read<T>(bytes) builds it - just
        // not as a field of an object that is being filled.
        else if constexpr (must_be_built_v<U, Ar>) static_assert(dependent_false<T>,
            "ser: this type cannot be filled in place - a const field, most likely - and "
            "it is not move-assignable either, so it cannot be built and assigned. On its "
            "own it still reads: ser::read<T>(bytes) builds it. As a FIELD of another type "
            "it needs one of - drop the const, add a ser_read hook, or give the ENCLOSING "
            "type a ser_make so this one is built rather than filled.");

        else if constexpr (::std::is_aggregate_v<U> && !can_enumerate_members_v<U>)
            static_assert(dependent_false<T>,
                "ser: this type is an aggregate whose field count cannot be worked out. "
                "Two things cause it. A C ARRAY field elides braces, so counting sees its "
                "elements rather than the field. A BASE CLASS is an element of aggregate "
                "initialization but not of a structured binding, so counting sees one "
                "field too many. Declare the count in the class:\n"
                "  using ser_members = ser::members<N>;\n"
                "That settles the array, and a base with no data members of its own. A base "
                "that HAS data members cannot be decomposed at all - give the type a "
                "ser_visit hook.");

        else if constexpr (nongeneric_any_hook_v<U, Ar>) return errc::invalid_value;
        else if constexpr (builtin::looks_pointer_like<U>()) {
            builtin::deny_pointer_like<U>();
            return errc::invalid_value;
        }
        else static_assert(dependent_false<T>,
            "ser: no way to deserialize this type.\n"
            "  Aggregate with public fields?  nothing needed\n"
            "  Private fields?                using ser_members = ser::members<N>; + SER_FRIEND\n"
            "  Has a constructor?             SER_MAKE_FROM(Type, a, b, c);\n"
            "  Not your type?                 specialize ser::serializer<T>");
    }

    // ── building an object out of the stream, field by field ───────────────────
    // BRACES, never parentheses. [dcl.init.list]/4 guarantees the initializer clauses are
    // evaluated left to right, so field 0 reads the first bytes of the record.
    // Parentheses leave the order unspecified - a format that depends on the compiler and
    // passes every test on the machine that produced it. test/t_order.cpp pins it.
    //
    // Every clause is a prvalue coming straight out of dispatch_make, so each field is
    // built once, in its final storage: const fields, fields with no default constructor
    // and fields that cannot be moved all work.

    // ── one field is not always one clause ─────────────────────────────────────
    // A C array field cannot take a clause of its own: no function returns an array, so
    // there is no prvalue of type int[3] to hand it. What an array field DOES accept is
    // one clause per element, through brace elision - the very mechanism that makes
    // counting fields untrustworthy is what makes building them possible.
    //
    // So an array field of extent N contributes N clauses, recursively for arrays of
    // arrays, and every other field contributes one. Elision then fills the array
    // element by element and leaves the remaining clauses to the next field. Nothing
    // about the bytes changes: the write side walks the same array through
    // builtin::write_array, element by element, in the same order.
    template <class F>
    struct flatten_field { using type = type_list<F>; };

    template <class E, ::std::size_t N>
    struct flatten_field<E[N]> {
        using type = repeat_list_t<typename flatten_field<E>::type, N>;
    };

    template <class T, class Seq> struct flat_fields;

    template <class T, ::std::size_t... I>
    struct flat_fields<T, ::std::index_sequence<I...>> {
        using type = cat_lists_t<typename flatten_field<member_type_t<T, I>>::type...>;
    };

    template <class T>
    using flat_fields_t =
        typename flat_fields<T, ::std::make_index_sequence<member_count_v<T>>>::type;

    // -Wmissing-braces is exactly backwards here: the flattened list gives an array field
    // one clause per element, and brace elision filling that array is the mechanism, not
    // an oversight. Adding the braces clang asks for would be the bug. Silenced narrowly
    // so the warning keeps working everywhere else; the guard is needed because GCC and
    // MSVC would report the clang pragma itself as unknown.
    // -Wconversion is the second one, and it is GCC that reports it: a BIT-FIELD is
    // initialized here from a prvalue of its own DECLARED type - `unsigned` for
    // `unsigned a : 4` - so the clause narrows to the width and GCC says the value may
    // change. It cannot: the write side read that same bit-field and widened it, so what
    // comes back always fits. Suppressed rather than cast, because there is no type to
    // cast to - a width is not a type - and nothing else here can convert at all: every
    // clause is exactly dispatch_make<Fs>, whose result type IS Fs. A narrowing that is a
    // real bug is refused by the braces themselves, which is what SER_MAKE_FROM names.
#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wmissing-braces"
#   pragma clang diagnostic ignored "-Wconversion"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wconversion"
#endif
    template <class T, class... Fs>
    constexpr T make_from_clauses(reader auto& ar, type_list<Fs...>) {
        return T{ dispatch_make<Fs>(ar)... };
    }
#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

    // One clause per field, and the fields come as a pack straight off the ladder rather
    // than index by index - member_type_t would ask tuple_element for each of them, for a
    // list the ladder already produced whole.
    template <class T, class... Fs>
    constexpr T make_one_clause_per_field(reader auto& ar, type_list<Fs...>) {
        // No flattening here, so an array field has nowhere to go: it would need a
        // clause of its own, and no expression yields an array to fill one with.
        // Saying so beats the raw "no matching overloaded function" from below.
        static_assert(!(::std::is_array_v<Fs> || ... || false),
            "ser: this type is not an aggregate and has a C array field, so it cannot be "
            "built one clause per field - braces here call a CONSTRUCTOR, and nothing "
            "returns an array to pass to it. Write ser_make by hand and name the elements:\n"
            "  static T ser_make(ser::reader auto& ar) {\n"
            "      return T{ ser::read_field<E>(ar), ser::read_field<E>(ar), ... };\n"
            "  }\n"
            "Braced, so the reads stay in order. Or hold the field as std::array, which "
            "is a class and can be built and returned like any other field.");

        return T{ dispatch_make<Fs>(ar)... };
    }

    // Flattening is for AGGREGATES only. Braces on anything else call a constructor, and
    // a constructor takes one argument per field - handing it three ints for an array
    // parameter would be a different overload, not brace elision.
    template <class T>
    constexpr T make_by_fields(reader auto& ar) {
        check_walkable_fields<T>();
        if constexpr (::std::is_aggregate_v<T>)
            return make_from_clauses<T>(ar, flat_fields_t<T>{});
        else
            return make_one_clause_per_field<T>(ar, field_types_t<T>{});
    }

    // ── the same fields, through parentheses ───────────────────────────────────
    // For a type whose braces are a list - a std::initializer_list constructor - braces
    // reach the wrong constructor, and parentheses are the only way to the right one.
    // Parentheses do not order their arguments, so the fields are read into a tuple
    // first: that IS a braced list, and [dcl.init.list]/4 orders its clauses left to
    // right. Then apply calls the constructor with what was already read.
    //
    // The price is one move per field, which is why braces stay the default.
    template <class T, class... Fs>
    constexpr T make_paren_from_list(reader auto& ar, type_list<Fs...>) {
        static_assert(!(::std::is_array_v<Fs> || ... || false),
            "ser: this type has a C array field, and a constructor cannot be handed an "
            "array by value. Read into an existing object instead - T obj; ser::in{bytes}(obj); "
            "- or hold the field as std::array, which is a class and can be passed.");

        ::std::tuple<Fs...> fields{ dispatch_make<Fs>(ar)... };
        return ::std::apply([](auto&... f) { return T(::std::move(f)...); }, fields);
    }

    template <class T>
    constexpr T make_paren_by_fields(reader auto& ar) {
        return make_paren_from_list<T>(ar, field_types_t<T>{});
    }

    // The one array field that must NOT be flattened: one with a serializer of its own.
    // The write side would use that hook, and reading the elements back one by one would
    // then be a different format. Such a type keeps the refusal below.
    template <class T, class Ar, ::std::size_t... I>
    consteval bool array_field_has_own_serializer(::std::index_sequence<I...>) {
        return ((::std::is_array_v<member_type_t<T, I>>
                 && has_custom_serializer_v<member_type_t<T, I>, Ar>) || ... || false);
    }

    template <class T, class Ar>
    inline constexpr bool aggregate_makeable_v =
        ::std::is_aggregate_v<T> && can_enumerate_members_v<T>
        && !array_field_has_own_serializer<T, Ar>(::std::make_index_sequence<member_count_v<T>>{});

    // Returns by value so that types without a default constructor, with const fields,
    // or non-movable ones can be built directly into their final storage. Errors travel
    // as an exception because there is no room for a return code next to the value -
    // ser::read catches it at the boundary. This is the only place with exceptions.
    template <class T, reader Ar>
        requires (!::std::is_array_v<T>)
    constexpr T dispatch_make(Ar& ar) {
        check_hooks<T, Ar>();

        // A ser_make that reads its fields with read_field recurses through here without
        // ever touching dispatch_read, so this path needs its own guard.
        depth_guard g{ar};
        if (!g.entered()) throw_error(errc::depth_exceeded, ar.position());

        if constexpr (builtin::denied_v<T>) {
            builtin::deny<T>();
            throw_error(errc::invalid_value, ar.position());
        }

        else if constexpr (access::has_trait_make_v<T, Ar>) return serializer<T>::make(ar); // 1
        else if constexpr (access::has_member_make_v<T, Ar>) return access::call_make<T>(ar); // 2
        else if constexpr (has_adl_make_v<T, Ar>) return adl_barrier::call_make(ar, ::ser::tag<T>{}); // 3

        // Every one of those three returns the hook's prvalue straight out of a return
        // statement, so the object is built once, in the caller's storage. That is what
        // makes a non-movable type readable at all.

        // `return x;` is NRVO - copy elision that the standard permits but does not
        // guarantee, so a move constructor must exist as the fallback. It is asked for in
        // the CONDITION rather than checked inside, so that a default constructible but
        // non-movable type falls through to step 4b instead of being refused here: 4b
        // builds T from prvalues and never moves it, so such a type is its case, not an
        // error. Until 4b lands the fall-through reaches the refusal below, unchanged.
        // The third condition is there for the same reason as the other two: step 4
        // delegates to dispatch_read, and dispatch_read cannot write into a field that is
        // const or a bit-field. Declining here is what sends such a type to 4b, which
        // builds every field by value and needs neither. It also breaks what would
        // otherwise be a cycle, since step 9 of dispatch_read comes back here.
        else if constexpr (::std::default_initializable<T> && ::std::move_constructible<T>
                           && !must_be_built_v<T, Ar>) {                                      // 4
            T x{};
            if (const auto e = dispatch_read<T>(ar, x); e != errc::ok)
                throw_error(e, ar.position());
            return x;
        }
        // Step 4b. An aggregate that step 4 declined: no default constructor, a const
        // field, or nothing to move it with. Aggregate initialization needs none of the
        // three - it builds every field in place from a prvalue.
        else if constexpr (aggregate_makeable_v<T, Ar>)                                       // 4b
            return make_by_fields<T>(ar);

        else if constexpr (must_be_built_v<T, Ar> && !::std::is_aggregate_v<T>)
            static_assert(dependent_false<T>,
            "ser: this type cannot be filled in place - a const field or a BIT-FIELD, and "
            "neither can be written through a reference - and it is not an aggregate, so it "
            "cannot be built from its fields either. Braces are what initialize a field by "
            "value, and only an aggregate has them. Give it SER_DESCRIBE_MAKE(Type, a, b): "
            "its read side builds through the constructor and reads each field BY VALUE, "
            "which a bit-field and a const field both accept. SER_MAKE_FROM alone is not "
            "enough - it supplies only the read side, and the pairing rule wants both.");

        else if constexpr (::std::default_initializable<T>) static_assert(dependent_false<T>, // 4c
            "ser: this type is default constructible but not movable, so it cannot be "
            "returned by value. Read it in place instead: T obj; ser::in{bytes}(obj). "
            "Or give it a ser_make hook - a hook's result is returned directly and is "
            "never moved.");

        else if constexpr (::std::is_aggregate_v<T> && can_enumerate_members_v<T>)           // 4d
            static_assert(dependent_false<T>,
                "ser: this type has an array field with a serializer of its own, and no way "
                "to be constructed. An array field is normally built one element per "
                "initializer clause, but that would ignore the array's own serializer and "
                "read a different format than was written. Give the type a default "
                "constructor and read into it - T obj; ser::in{bytes}(obj); - or give the "
                "type itself a ser_make hook.");

        // 4e. The same refusal the other two ladders have, and it belongs here most of
        // all: a const field is what sends a type down the make path in the first
        // place, so an aggregate with a const ARRAY field arrives here and nowhere
        // else. Without this rung it would get the generic message below, whose three
        // suggestions are all about something other than what is wrong.
        else if constexpr (::std::is_aggregate_v<T> && !can_enumerate_members_v<T>)      // 4e
            static_assert(dependent_false<T>,
                "ser: this type is an aggregate whose field count cannot be worked out, "
                "so it cannot be built field by field. A C ARRAY field elides braces, so "
                "counting sees its elements rather than the field; a BASE CLASS is an "
                "element of aggregate initialization but not of a structured binding. "
                "Declare the count in the class:\n"
                "  using ser_members = ser::members<N>;\n"
                "A base that has data members of its own cannot be decomposed at all - "
                "give the type a ser_make hook instead.");

        else if constexpr (nongeneric_any_hook_v<T, Ar>) throw_error(errc::invalid_value, ar.position());
        else if constexpr (builtin::looks_pointer_like<T>()) {
            builtin::deny_pointer_like<T>();
            throw_error(errc::invalid_value, ar.position());
        }
        else static_assert(dependent_false<T>,
            "ser: don't know how to construct this type.\n"
            "  Has a constructor?          SER_MAKE_FROM(Type, a, b, c);\n"
            "  Not your type?              ser::serializer<T>::make\n"
            "  Has non-public fields?      SER_MAKE_FROM, or friend ser::access + members<N>");
    }

} // namespace ser::detail

namespace ser {

    // For hand-written ser_make: reads one field without naming detail::dispatch_make.
    template <class F, reader Ar>
    [[nodiscard]] constexpr F read_field(Ar& ar) {
        return detail::dispatch_make<F>(ar);
    }

} // namespace ser

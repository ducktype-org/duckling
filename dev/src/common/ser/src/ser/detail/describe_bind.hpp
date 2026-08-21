#pragma once

#include <ser/access.hpp>
#include <ser/config.hpp>

#include <ser/detail/meta.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser::detail {

    inline constexpr ::std::size_t no_count = static_cast<::std::size_t>(-1);

    // The ladder's limit, and it must match MAX_MEMBERS in tools/gen_ladder.py.
    inline constexpr ::std::size_t max_members = 64;

    // ── the counting probe ────────────────────────────────────────────────────
    // Converts to anything and is deliberately never defined: it appears only inside a
    // requires-expression, where the initialization is never evaluated.
    // Clang reports -Wundefined-inline for it, and here the warning is wrong rather than
    // merely noisy: an undefined inline function is a link error waiting to happen, and
    // this one can never be called - it is named only inside requires-expressions, whose
    // initializations are unevaluated operands. Clang counts being CHOSEN by overload
    // resolution as being used, which is why only conversions to CLASS types are reported
    // (those are the ones that reach a member through copy-initialization). Defining it
    // would mean returning a T that cannot be built for every T, so the declaration stays
    // and the warning is silenced exactly here. Measured on Clang 19.1 and 22.1.
#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wundefined-inline"
#endif
    struct any_init {
        template <class T>
        constexpr operator T() const;                       // NOT defined, on purpose
    };
#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

    // ── one probe, and why one is enough ──────────────────────────────────────
    // T{ p, p, p } - the largest N that compiles is the field count. Each clause
    // COPY-INITIALIZES its member, and copy-initialization is the context where a
    // conversion function is a first-class path: reaching a member of class type M uses
    // `operator M` directly and never touches M's own constructors. So the probe reaches
    // a member whatever its constructors look like - explicit, multi-argument, or absent.
    //
    // A clause in BRACES is a different context - copy-LIST-initialization - and there
    // the same route has to go through M's copy constructor, because list-initialization
    // considers M's constructors and nothing else. Whether that route is available, and
    // whether it then ties with M's own converting constructor, is where the compilers
    // part company. Measured, for
    //
    //     struct Id { int v; Id(int); };   struct S { Id i; int n; };
    //
    // MSVC and GCC call the braced clause ambiguous - between Id's copy and move
    // constructors - and Clang resolves it. A braced probe therefore answers differently
    // per compiler, and every way of making it answer the same everywhere was measured to
    // cost coverage. So there is one probe. The unbraced count came out IDENTICAL on
    // MSVC 14.51, GCC 13.3 and Clang 22 for every one of 44 measured shapes that does not
    // contain a C array.
    //
    // THE PRICE IS C ARRAYS, and it is paid deliberately. An unbraced clause is subject
    // to brace elision, and elision happens exactly where the member cannot be copy-
    // initialized from the clause. any_init converts to anything, so a scalar, a class or
    // a nested aggregate never elides - but nothing converts to `int[3]`, so an array
    // field consumes one clause PER ELEMENT and the count comes out too large. A C array
    // field in an automatically counted aggregate is therefore not supported. The count
    // is wrong, and because the arity of a structured binding is CHECKED against the type
    // and never deduced, that can only ever surface as a compile error inside ladder.inc -
    // never as wrong bytes. Declaring `using ser_members = ser::members<N>;` skips the
    // probe, and such a type then walks and serializes exactly as any other.
    template <class T, ::std::size_t... I>
    consteval bool init_with() {
        return requires { T{ (void(I), any_init{})... }; };
    }

    template <class T, ::std::size_t... I>
    consteval bool init_with_seq(::std::index_sequence<I...>) {
        return init_with<T, I...>();
    }

    // ── the scan ──────────────────────────────────────────────────────────────
    // Which clause counts are valid forms ONE CONTIGUOUS RUN. Below it a member without
    // a default constructor is left with no initializer; above it there are simply too
    // many clauses. So the largest valid N is found by walking up and stopping one past
    // the last success, and Best carries that success along as a template argument
    // because only a template argument can drive the `if constexpr` that stops.
    //
    // A recursion and not a fold, and that is not a matter of taste. MSVC answers C1202,
    // "dependency context too complex", at roughly a thousand instantiations of the probe
    // in one translation unit; a fold over the whole range spends 66 of them per type,
    // which is SIXTEEN aggregates in a file before the compiler stops - measured, not
    // feared. The scan spends about as many as the type has fields.
    template <class T, ::std::size_t N, ::std::size_t Best>
    consteval ::std::size_t scan_arity() {
        if constexpr (N > max_members + 1)
            return Best;
        else if constexpr (init_with_seq<T>(::std::make_index_sequence<N>{}))
            return scan_arity<T, N + 1, N>();                    // keep going
        else if constexpr (Best != no_count)
            return Best;                                        // one past the top
        else
            return scan_arity<T, N + 1, no_count>();            // nothing has fit yet
    }

    // Probed one PAST the ladder's limit on purpose. A type whose braces accept any
    // number of clauses - a std::initializer_list constructor - never stops, and
    // reporting max_members + 1 tells it apart from a type that genuinely has
    // max_members fields. Nothing else can produce that number.
    template <class T>
    inline constexpr ::std::size_t elided_arity_v = scan_arity<T, 0, no_count>();

    // ── "are these braces a LIST?" ────────────────────────────────────────────
    // Not part of counting - the probe above never puts a clause in braces. This is for
    // SER_MAKE_FROM, which expands to braces and has to refuse a type whose braces reach
    // an initializer_list constructor instead of the fields. A braced list one clause past
    // the ladder's limit answers it: a constructor stops at its own arity, an
    // initializer_list constructor never stops, and nothing else can say yes. It needs no
    // convention and no element type, and a variadic constructor template does not fool
    // it, because a braced-init-list is a non-deduced context.
    //
    // It costs max_members + 1 clauses, so it is asked only about the types SER_MAKE_FROM
    // is written on - never once per field of every counted aggregate.
    template <class T, ::std::size_t... I>
    consteval bool list_init_with() {
        return requires { T{ { (void(I), any_init{}) }... }; };
    }

    template <class T, ::std::size_t... I>
    consteval bool list_init_with_seq(::std::index_sequence<I...>) {
        return list_init_with<T, I...>();
    }

    template <class T>
    inline constexpr bool accepts_any_length_v =
        list_init_with_seq<T>(::std::make_index_sequence<max_members + 1>{});

    // "Braces here do not mean fields."
    template <class T>
    inline constexpr bool arity_saturated_v = accepts_any_length_v<T>;

    // ── does this type have a base class? ─────────────────────────────────────
    // Aggregate initialization treats a base class as an ELEMENT - D{ B{1}, 2 } - while a
    // structured binding sees only data members, and refuses outright when the base has
    // any of its own. So a base makes the probed count wrong, silently, in a type that is
    // otherwise a perfectly ordinary aggregate.
    //
    // Detected the way Boost.PFR does it: a probe that converts to nothing except a base
    // of T. If the first element accepts it, the first element is a base.
    template <class T>
    struct base_init {
        template <class U>
            requires (!::std::is_same_v<U, T> && ::std::is_base_of_v<U, T>)
        constexpr operator U() const;                       // NOT defined, on purpose
    };

    template <class T>
    inline constexpr bool has_base_v = requires { T{ base_init<T>{} }; };

    // ── can we enumerate this type's fields? ──────────────────────────────────
    // Asymmetric on purpose: false means "I do not know", never "it has no fields".
    // Every caller treats false as "ask the user", so a wrong false costs a message and
    // a wrong true costs a format.
    //
    // The branches are `if constexpr` and not a conjunction, and that is invariant 7:
    // `&&` instantiates both operands, and a type that has its own serializer may be
    // undecomposable outright (one deriving from std::unique_ptr, say). The probe is a
    // requires-expression, so an undecomposable type is a substitution failure rather
    // than a hard error - but the order still matters, because the cheap structural
    // answers must come first.
    //
    // No archive parameter, and that is deliberate (see CLAUDE.md): this answers "do I
    // understand this type's fields", which no archive changes. Whether a field walk is
    // the right thing to DO is the ladder's question, and the ladder has already
    // consulted every hook by the time it asks this one.
    template <class T>
    consteval bool can_enumerate_impl() {
        using U = ::std::remove_cv_t<T>;

        if constexpr (access::names_member_count_v<U>)
            return true;                                    // the author said so
        else if constexpr (::std::is_union_v<U> || ::std::is_array_v<U> || !::std::is_class_v<U>)
            return false;
        else if constexpr (!::std::is_aggregate_v<U>)
            return false;                                   // cannot probe a non-aggregate
        // No check for a polymorphic type: a class with virtual functions is never an
        // aggregate, so the line above has already refused it.
        else if constexpr (has_base_v<U>)
            return false;                                   // the probe would count the base
        else if constexpr (::std::is_empty_v<U>)
            return true;                                    // zero fields, and that is known
        else
            // Zero is not a count for a type that has fields - it means only the empty
            // clause list compiled - and a number past the ladder's limit has no rung
            // that could walk it.
            return elided_arity_v<U> != no_count && elided_arity_v<U> != 0
                && elided_arity_v<U> <= max_members;
    }

} // namespace ser::detail

namespace ser {

    // ── the public field-count surface ────────────────────────────────────────
    template <class T>
    inline constexpr bool can_enumerate_members_v =
        detail::can_enumerate_impl<::std::remove_cv_t<T>>();

    namespace detail {
        template <class T>
        consteval ::std::size_t member_count_of() {
            if constexpr (access::names_member_count_v<T>) return access::declared_member_count<T>();
            else if constexpr (::std::is_empty_v<T>)       return 0;
            else                                          return elided_arity_v<T>;
        }
    }

    // Only ask this when can_enumerate_members_v<T> is true. It is a plain value rather
    // than a hard error otherwise, because dispatch needs to test the guard first.
    template <class T>
    inline constexpr ::std::size_t member_count_v =
        detail::member_count_of<::std::remove_cv_t<T>>();

    // ── visiting ──────────────────────────────────────────────────────────────
    // The arity lives here rather than in ser::access because the count needs the ladder
    // (member_type_t below is built out of it) and the ladder must therefore need
    // nothing. access::visit_members_n takes N explicitly for exactly that reason.
    template <class T, class F>
    constexpr decltype(auto) visit_members(T&& obj, F&& f) {
        using U = ::std::remove_cvref_t<T>;
        // There is NO check of a declared count against the probe, and that is a measured
        // conclusion rather than an omission. With one probe no comparison is sound,
        // because the probe can err in BOTH directions:
        //
        //   overshoot - a C array field takes one clause per element:
        //               struct { int a; int b[3]; }      -> probe 4, fields 2
        //   undershoot - the scan STALLS at the first member no clause can initialize,
        //               and a non-const reference member is exactly that:
        //               struct { int a; int& r = g; int c = 0; }  -> probe 1, fields 3
        //               struct { int& r = g; int n; }             -> probe 0, fields 2
        //
        // So `declared > probed` is not evidence of a wrong declaration, and neither is
        // the other direction. A wrong ser::members<N> is caught where it cannot be wrong
        // about: the arity of a structured binding is CHECKED against the type, so it is
        // a compile error inside ladder.inc. That is the same bargain the C array case
        // takes - the compiler's message where the library cannot honestly judge.
        static_assert(can_enumerate_members_v<U>,
            "ser: cannot enumerate this type's fields. Declare the count in the class:\n"
            "  using ser_members = ser::members<N>;   (with SER_FRIEND for private fields)\n"
            "or give the type a hook: ser_visit, or ser_write + ser_read.\n"
            "A C array field is the common reason: it elides braces, so the count comes "
            "out too large and ser::members<N> is what settles it.");
        return access::visit_members_n<member_count_v<U>>(
            ::std::forward<T>(obj), ::std::forward<F>(f));
    }

    // ── member_type_t ─────────────────────────────────────────────────────────
    // The Boost.PFR trick: tie the members into a tuple of references and read the types
    // back off it. Never called - only its return type is ever needed - so the
    // declval<T&> is harmless even for a type that cannot be created.
    template <class T>
    constexpr auto tie_members(T& obj) {
        return access::visit_members_n<member_count_v<T>>(
            obj, [](auto&... fields) { return ::std::tie(fields...); });
    }

    template <class T>
    using member_tuple_t = decltype(tie_members(::std::declval<::std::remove_cv_t<T>&>()));

    // The remove_cvref_t here has a consequence worth naming, because it is invisible at
    // every call site: a member of REFERENCE type arrives as its referenced type, so the
    // "cannot serialize a reference" refusal in pointer_deny.hpp is never reached for a
    // reference FIELD. It is reached for a reference asked about directly - measured,
    // ser::read_field<const int&> fires it - so the guard is right; nothing ever hands it
    // a reference. The cost is real and measured: struct S { const int& r; int n; } reads
    // back through step 4b, whose braces live in a return statement, and a temporary bound
    // to a reference member is NOT lifetime-extended there, so S::r dangles. ASan calls it
    // stack-use-after-return; no compiler warns.
    //
    // A reference field fails differently depending on which read path it reaches, and
    // both ways are measured. Without an initializer the type is not default-constructible,
    // so step 4b builds it and `const T&` dangles as above while `T&` is a compile error
    // (a prvalue does not bind to a non-const lvalue reference). WITH a default member
    // initializer the type IS default-constructible, so step 4 fills it instead: `const T&`
    // is then a compile error, and `T&` compiles and quietly OVERWRITES whatever the
    // default initializer pointed at - measured, a global went from 111 to 222 during a
    // read, and the reference came back pointing at the default target rather than the one
    // that was written. That is what pointer_deny.hpp means by "a reference can never be
    // rebound". It takes an explicit ser::members<N> to get there, because the probe stalls
    // on such a member and refuses the type on its own.
    //
    // Dropping the remove_cvref_t is not the fix. By the time a field reaches here its
    // declared type is already gone: ladder.inc hands the bindings to `f(m0, m1, ...)`
    // whose parameters are `auto&`, and that deduction turns BOTH a `const int&` member
    // and a `const int` member into `const int&`. Measured at all three stations - the
    // binding tells them apart, the parameter does not, remove_cvref_t is merely last.
    // So a guard here would refuse every const field too, and those round-trip today.
    // Refusing a reference field needs the ladder to surface decltype(m0) per field,
    // which is a change to tools/gen_ladder.py, not to this line.
    namespace detail {
        // field_decls_t -> the same fields with cv and references stripped, in both the
        // shapes the rest of the library asks for.
        template <class L> struct decl_field_types;

        template <class... Ds>
        struct decl_field_types<type_list<Ds...>> {
            using tuple = ::std::tuple<::std::remove_cvref_t<typename Ds::type>...>;
            using list  = type_list<::std::remove_cvref_t<typename Ds::type>...>;
        };
    }

    // The same fields as field_types_t, but as they were DECLARED - `int&` for a
    // reference member, `const int` for a const one, plus whether a non-const lvalue
    // reference can bind to each. field_types_t cannot carry either, and the note on
    // member_type_t above says why: it is not that remove_cvref_t throws it away, it is
    // that handing a binding to a function parameter already had.
    //
    // Only the guards use this. Every serializing path keeps using field_types_t, which
    // must stay stripped: a `const int` field has to dispatch as `int`.
    template <class T>
    using field_decls_t = decltype(access::field_decls_n<member_count_v<::std::remove_cv_t<T>>>(
        ::std::declval<::std::remove_cv_t<T>&>()));

    // Taken off field_decls_t and not off member_tuple_t, and that is not a cosmetic
    // choice: member_tuple_t goes through std::tie, which needs a non-const reference per
    // field, and nothing binds one to a BIT-FIELD - so member_tuple_t<T> does not compile
    // at all for such a type. field_decls_t asks decltype on the binding instead, which
    // works for every field there is. The answers agree everywhere both are available:
    // tie turns a const field into `const int&` and a declared type is `const int`, and
    // remove_cvref_t flattens both to `int`.
    //
    // It matters because the BUILD path needs these types, and the build path is exactly
    // what reads a type the walk cannot fill - a bit-field being the case.
    template <class T, ::std::size_t I>
    using member_type_t =
        ::std::tuple_element_t<I, typename detail::decl_field_types<field_decls_t<T>>::tuple>;

    // The same types as a pack rather than one index at a time - what a caller needs when
    // it wants every field type at once, and what dispatch hands to the build path.
    template <class T>
    using field_types_t = typename detail::decl_field_types<field_decls_t<T>>::list;


} // namespace ser

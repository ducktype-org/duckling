#pragma once

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/pool/context.hpp>
#include <ser/stream/header.hpp>

#include <ser/detail/dispatch.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace ser {

    // ── options ───────────────────────────────────────────────────────────────
    // What goes around the payload. The envelope is described in stream/header.hpp; this
    // is the switch that puts one there.
    //
    // header DEFAULTS TO FALSE IN M1, and that is a deliberate deviation from the design,
    // which has it true. The reason is that M1 has no pools: without them a headerless
    // stream loses version, platform and schema validation and nothing else, and
    // ser::write is documented to produce exactly the payload - buf.size() is the bytes
    // the object needed, which is what makes the wire testable byte for byte.
    //
    // It flips with pools. A headerless stream carrying pools cannot detect that the
    // reader registered a different Ctx than the writer: the trailing pool_offset is read
    // as payload, silently. That is what the M2 completeness check and the schema_hash of
    // the context are for, and neither can run without a header - so M2 turns this on by
    // default and refuses `header = false` together with pools.
    //
    // user_magic brands the stream as yours: four bytes after "SER\0" that a reader
    // passes back in, so somebody else's ser stream is rejected as bad_magic rather than
    // reaching your schema check.
    //
    // checksum is not here yet. The header carries payload_crc and M1 writes 0 into it;
    // the flag arrives with crc32c.hpp in M2, because a flag that quietly does nothing is
    // worse than an absent one.
    struct options {
        bool            header     = false;
        ::std::uint32_t user_magic = 0;
    };

    namespace detail {

        // The envelope on the throwing path. Every read funnels through read_or_throw, and
        // there a bad header is an exception rather than a code - the code paths get it
        // back from the catch in ser::read. Reading it also POSITIONS the archive at the
        // payload, so the caller just carries on.
        template <class T, class Ctx, class Ar>
        void read_envelope(Ar& ar, const options& opt) {
            stream_header h{};
            if (const auto e = read_header(ar, h, opt.user_magic); e != errc::ok)
                throw_error(e, ar.position());
            if (const auto e = check_header<T, Ctx>(h); e != errc::ok)
                throw_error(e, ar.position());
        }

    } // namespace detail

    // ── owned<T, Ctx> ─────────────────────────────────────────────────────────
    // An AGGREGATE on purpose. ser::read builds it as
    //     owned<T, Ctx>{ dispatch_make<T>(ar), std::move(ctx) }
    // where the prvalue initializes `value` directly - no move, and it compiles for
    // types with const fields. Building it as `owned r; r.value = ...` would throw
    // away the copy elision the whole make path exists for.
    //
    // Field order matters: `value` is declared first, so it is destroyed first. In
    // M2/M3 the object refers into the pools held by `ctx`, and it must die before them.
    template <class T, class Ctx>
    struct owned {
        T value;
        SER_NO_UNIQUE_ADDRESS Ctx ctx;

        constexpr T*       operator->()       noexcept { return &value; }
        constexpr const T* operator->() const noexcept { return &value; }
        constexpr T&       operator*()        noexcept { return  value; }
        constexpr const T& operator*()  const noexcept { return  value; }

        template <class P> constexpr P& pool() { return ctx.template pool<P>(); }

        // Taking the object out is safe exactly when dropping the context is safe: always
        // for an empty one, and for a pooled one only once every pool entry has been
        // claimed - which is the M2 consumed() check, and where this requires clause
        // widens. The && forces std::move(r).take(), so the call site says it consumes.
        [[nodiscard]] constexpr T take() && requires (::std::is_empty_v<Ctx>) {
            static_assert(::std::move_constructible<T>,
                "ser: owned::take needs a movable T - an object that cannot be moved "
                "cannot be relocated out of the bundle. Build it where it belongs: "
                "ser::in ar{bytes}; T obj{ ser::read_field<F>(ar), ... };");
            return ::std::move(value);
        }
    };

    // ── read ──────────────────────────────────────────────────────────────────
    // Both entry points return owned<T, Ctx>, never a bare T - even when Ctx is empty.
    // Pools in M2 add r.pool<P>() without touching a single call site, because *r and
    // r-> already work.

    // Errors travel as an exception, which buys the one thing ser::read cannot have:
    // the return type equals the type of the returned prvalue, so copy elision is
    // guaranteed end to end and T never has to be movable.
    template <class T, class Ctx = no_context>
    [[nodiscard]] owned<T, Ctx> read_or_throw(::std::span<const ::std::byte> bytes,
                                              options opt = {}) {
        static_assert(!::std::is_array_v<T>,
                      "ser::read_or_throw: cannot return a C array by value. Read it in "
                      "place: T arr; ser::in{bytes}(arr).");
        Ctx     ctx{};
        in<Ctx> ar{bytes, ctx};
        if (opt.header) detail::read_envelope<T, Ctx>(ar, opt);
        // Left-to-right evaluation is guaranteed for braced init, so `ar` is done being
        // used before `ctx` is moved out from under it.
        return owned<T, Ctx>{ detail::dispatch_make<T>(ar), ::std::move(ctx) };
    }

    // ── read_or_throw_force ───────────────────────────────────────────────────
    // The bare object, with no bundle around it. Same guarantee as read_or_throw about
    // construction - the prvalue out of dispatch_make initializes the caller's object
    // directly, so a type that cannot be moved works here too - and none of the ceremony:
    //
    //     const Config cfg = ser::read_or_throw_force<Config>(bytes);
    //
    // What it drops is the context, and that is the whole reason the other two do not.
    // The context is created here and destroyed on the way out, so anything in the object
    // that points into it is dangling before the caller sees it - not an error code, not
    // an exception, undefined behaviour. Hence the constraint: an EMPTY context has
    // nothing to point into, and there is nothing to lose by dropping it.
    //
    // That covers most calls today, and will keep covering the ones that use no pools -
    // which is why this is worth having rather than making everyone spell out
    // std::move(r).take(). The constraint is the same one on owned::take(), and it widens
    // the same way in M2: once every pool entry is claimed the context is scaffolding,
    // and dropping it is safe by the same argument. Until then, a stateful context means
    // read_or_throw, and the object stays bundled with the pools it may point into.
    template <class T, class Ctx = no_context>
        requires (::std::is_empty_v<Ctx>)
    [[nodiscard]] T read_or_throw_force(::std::span<const ::std::byte> bytes, options opt = {}) {
        static_assert(!::std::is_array_v<T>,
                      "ser::read_or_throw_force: cannot return a C array by value. Read it "
                      "in place: T arr; ser::in{bytes}(arr).");
        Ctx     ctx{};
        in<Ctx> ar{bytes, ctx};
        if (opt.header) detail::read_envelope<T, Ctx>(ar, opt);
        return detail::dispatch_make<T>(ar);
    }

    template <class T, class Ctx = no_context>
    [[nodiscard]] result<owned<T, Ctx>> read(::std::span<const ::std::byte> bytes,
                                             options opt = {}) {
        // The bundle reaches the expected through a constructor parameter, and elision
        // never crosses one - so this path costs exactly one move of owned<T, Ctx>.
        static_assert(::std::move_constructible<T>,
                      "ser::read: T must be movable, because result<owned<T, Ctx>> has to "
                      "move the bundle into std::expected. For a type that cannot be moved "
                      "use ser::read_or_throw<T>(bytes) - it returns owned<T, Ctx> by value "
                      "and elides everything.");
        try {
            return read_or_throw<T, Ctx>(bytes, opt);
        } catch (const exception& e) {
            return result<owned<T, Ctx>>{ e.err() };
        }
    }

    // ── write ─────────────────────────────────────────────────────────────────
    // Appends. Into a growable buffer each call starts where the last one stopped, so
    // several objects written separately end up as one stream and buf.size() is always
    // exactly the bytes produced so far - nothing is trimmed and nothing stale is left
    // behind. Into a fixed buffer there is nothing to append to (a span's size() is
    // capacity), so each call fills it from the front.
    //
    //     ser::write(buf, header);         // buf: [header]
    //     ser::write(buf, payload);        // buf: [header][payload]
    //
    // Reading them back is the mirror image, and it is one archive rather than two
    // calls, because ser::read starts at the front of the span it is given and does not
    // report how far it got:
    //
    //     ser::in ar{bytes};
    //     ar(header, payload);
    //
    // One write is one call to finish(), which in M2 flushes that message's pools - so
    // appending several messages means several self-contained messages, not one message
    // in several pieces.
    template <class Buf, class T>
    [[nodiscard]] result<> write(Buf& buf, const T& x, options opt = {}) {
        out<Buf> ar{buf};
        const ::std::size_t start = ar.position();

        // Written twice when there is one, and that is what payload_size costs: the size
        // is not known until the payload is out, so the first copy reserves the 32 bytes
        // and the second one - after finish(), so that M2's pool data counts as payload -
        // patches them. ser::out::reset exists for exactly this.
        stream_header h{};
        if (opt.header) {
            h = stream_header::for_type<T>(opt.user_magic);
            if (const auto e = write_header(ar, h); e != errc::ok) return result<>{e, ar.position()};
        }
        const ::std::size_t payload_start = ar.position();

        if (const auto e = ar(x);        e != errc::ok) return result<>{e, ar.position()};
        if (const auto e = ar.finish();  e != errc::ok) return result<>{e, ar.position()};

        if (opt.header) {
            const ::std::size_t end = ar.position();
            h.payload_size = static_cast<::std::uint64_t>(end - payload_start);
            ar.reset(start);
            if (const auto e = write_header(ar, h); e != errc::ok) return result<>{e, ar.position()};
            ar.reset(end);
        }
        return result<>{};
    }

} // namespace ser

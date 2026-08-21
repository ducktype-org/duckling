#pragma once

#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/detail/meta.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <type_traits>

namespace ser::detail {

	// Everything ser::out and ser::in hold in common: the stream position, the depth
	// counter and the context. It exists so that the answer to
	// "how is the context stored" lives in exactly one place instead of six.
	template<class Ctx>
	class archive_base {
	public:
		using context_type = Ctx;

		[[nodiscard]] constexpr ::std::size_t position() const noexcept { return pos; }

		// ── depth guard (data-dependent recursion) ────────────────────────────
		[[nodiscard]] constexpr ::std::size_t depth() const noexcept { return nesting; }

		[[nodiscard]] constexpr bool pushDepth() noexcept {
			if (nesting >= config_global::MAX_DEPTH) return false;
			++nesting;
			return true;
		}

		constexpr void popDepth() noexcept {
			SER_ASSERT(nesting != 0, "ser: archive_base::popDepth: unbalanced depth guard");
			--nesting;
		}

		// ── pools ─────────────────────────────────────────────────────────────
		template<class P>
		P& pool() {
			static_assert(
				DEPENDENT_FALSE<P>,
				"ser: this archive has no pools. Pools are not implemented yet; leave the "
				"context parameter at its default, ser::no_context."
			);
		}

	protected:
		::std::size_t pos = 0;

		// Only an empty context can be conjured out of nothing. A stateful one has to be
		// passed in, or ctx would be a null pointer.
		constexpr archive_base() noexcept requires(::std::is_empty_v<Ctx>) = default;

		constexpr explicit archive_base(Ctx& c) noexcept {
			if constexpr (::std::is_empty_v<Ctx>)
				ctx = c;
			else
				ctx = &c;
		}

		// The one place in the library that knows how the context is stored.
		[[nodiscard]] constexpr Ctx& context() noexcept {
			if constexpr (::std::is_empty_v<Ctx>)
				return ctx;
			else
				return *ctx;
		}

	private:
		// A reference is ALWAYS 8 bytes, which would break the "an empty context costs
		// nothing" guarantee. Empty contexts are stored by value (0 bytes under
		// SER_NO_UNIQUE_ADDRESS); everything else by pointer, because the archive
		// BORROWS the context - a copied pool would diverge from the one finish()
		// flushes, and the stream would reference entries nobody wrote.
		//
		// The condition is emptiness, not POOL_COUNT: what makes by-value storage safe
		// is having no state to lose, and a stateful context with zero pools would
		// silently discard every update the archive made to it.
		using ctx_storage = ::std::conditional_t<::std::is_empty_v<Ctx>, Ctx, Ctx*>;

		::std::size_t                     nesting = 0;
		SER_NO_UNIQUE_ADDRESS ctx_storage ctx{};
	};

}  // namespace ser::detail

#pragma once

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/no_unique_address.hpp>

#include <ser/config.hpp>
#include <ser/pool/context.hpp>

#include <cstddef>
#include <type_traits>

namespace ser::internal {

	/**
	 * @brief Everything ser::Out and ser::In hold in common: the stream position, the depth
	 * counter and the context. It exists so that the answer to
	 * "how is the context stored" lives in exactly one place instead of six.
	 */
	template<class Ctx>
	class ArchiveBase {
	public:
		using ContextType = Ctx;

		[[nodiscard]] constexpr ::std::size_t position() const noexcept { return pos; }

		/** @brief depth guard (data-dependent recursion) */
		[[nodiscard]] constexpr ::std::size_t depth() const noexcept { return nesting; }

		[[nodiscard]] constexpr bool pushDepth() noexcept {
			if (nesting >= ConfigGlobal::MAX_DEPTH) return false;
			++nesting;
			return true;
		}

		/** @brief Not noexcept: CORE_ASSERT throws base::Panic in a Dev build. */
		constexpr void popDepth() {
			CORE_ASSERT(nesting != 0, "ser: ArchiveBase::popDepth: unbalanced depth guard");
			--nesting;
		}

		/** @brief pools */
		template<class P>
		P& pool() {
			static_assert(
				::base::DEPENDENT_FALSE_V<P>,
				"ser: this archive has no pools. Pools are not implemented yet; leave the "
				"context parameter at its default, ser::NoContext."
			);
		}

	protected:
		::std::size_t pos = 0;

		/**
		 * @brief Only an empty context can be conjured out of nothing. A stateful one has to be
		 * passed in, or ctx would be a null pointer.
		 */
		constexpr ArchiveBase() noexcept requires(::std::is_empty_v<Ctx>) = default;

		constexpr explicit ArchiveBase(Ctx& c) noexcept {
			if constexpr (::std::is_empty_v<Ctx>)
				ctx = c;
			else
				ctx = &c;
		}

		/** @brief The one place in the library that knows how the context is stored. */
		[[nodiscard]] constexpr Ctx& context() noexcept {
			if constexpr (::std::is_empty_v<Ctx>)
				return ctx;
			else
				return *ctx;
		}

	private:
		/**
		 * @brief A reference is ALWAYS 8 bytes, which would break the "an empty context costs
		 * nothing" guarantee. So empty contexts are stored by value and everything else by
		 * pointer, because the archive BORROWS the context - a copied pool would diverge from
		 * the one finish() flushes. The condition is emptiness and not POOL_COUNT: a stateful
		 * context with zero pools would silently discard every update made to it.
		 */
		using CtxStorage = ::std::conditional_t<::std::is_empty_v<Ctx>, Ctx, Ctx*>;

		::std::size_t                nesting = 0;
		NO_UNIQUE_ADDRESS CtxStorage ctx{};
	};

}  // namespace ser::internal

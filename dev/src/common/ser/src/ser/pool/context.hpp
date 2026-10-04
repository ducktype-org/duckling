#pragma once

#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>

/**
 * @file
 * @brief The context an archive carries for pools.
 * @details A pool is storage shared by one whole message: a value that many objects refer to
 * is written once into the pool, and every occurrence writes only its index. The planned
 * ones are a string pool for `StrID` (#90002) and an object pool for `Box` / `Ref`, so
 * sharing and aliasing survive a round trip (#90003).
 *
 * None exists yet, so every archive runs with `NoContext`. The `Ctx` parameter is there so
 * adding a pool does not change the archive API.
 */

namespace ser {

	/**
	 * @brief The empty context. Archives take Ctx as a template parameter so that adding pools
	 * is additive - no call site changes shape. `POOL_COUNT == 0` is the compile-time
	 * switch that removes every pool branch from the generated code.
	 */
	struct NoContext final {
		static constexpr ::std::size_t POOL_COUNT = 0;

		constexpr Errc finish() noexcept { return Errc::Ok; }
	};

	// @TODO: #90002 implement context with the StrID pool
	/** @brief Not implemented yet. Declared so that `Context<...>` names a type. */
	template<class... Pools>
	struct Context;

	/**
	 * @brief out{buf} must work without the caller naming a context, and an archive stores an empty
	 * one BY VALUE - so this singleton exists only for the one-argument constructor to have
	 * something to bind to.
	 */
	inline NoContext no_context_instance{};

}  // namespace ser

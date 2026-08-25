#pragma once

#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>

namespace ser {

	// The empty context. Archives take Ctx as a template parameter so that adding pools
	// is additive - no call site changes shape. `POOL_COUNT == 0` is the compile-time
	// switch that removes every pool branch from the generated code.
	struct no_context {
		static constexpr ::std::size_t POOL_COUNT = 0;

		constexpr Errc finish() noexcept { return Errc::Ok; }
	};

	// Not implemented yet. Declared so that `context<...>` names a type.
	template<class... Pools>
	struct context;

	// out{buf} must work without the caller naming a context, and an archive stores an empty
	// one BY VALUE - so this singleton exists only for the one-argument constructor to have
	// something to bind to.
	inline no_context no_context_instance{};

}  // namespace ser

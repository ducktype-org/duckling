#pragma once

#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>

namespace ser {

    // The empty context. Archives take Ctx as a template parameter from day one so that
    // adding pools in M2 is additive - no call site changes shape. `pool_count == 0` is
    // the compile-time switch that removes every pool branch from the generated code.
    struct no_context {
        static constexpr ::std::size_t pool_count = 0;

        constexpr errc finish() noexcept { return errc::ok; }
    };

    // Real contexts arrive in M2. Declared here so that `context<...>` names a type today.
    template <class... Pools>
    struct context;

    // out{buf} must work without the caller naming a context. An archive stores an empty
    // context BY VALUE (0 bytes under SER_NO_UNIQUE_ADDRESS), so this singleton exists only
    // for the one-argument constructor to have something to bind to.
    inline no_context no_context_instance{};

} // namespace ser

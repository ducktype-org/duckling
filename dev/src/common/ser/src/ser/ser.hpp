#pragma once

/**
 * @file
 * @brief The one header a user of `ser` includes.
 * @details Everything a caller writes by hand is reachable from here: the entry points, the
 * hook vocabulary, the `SER_*` macros and the round-trip check. The only other headers a user
 * ever needs are the two opt-in adapter sets, `<ser/std/all.hpp>` and `<ser/base/all.hpp>`,
 * which stay separate so this header never pays for `<map>` to serialize something that has
 * nothing to do with maps.
 * @note Nothing under `ser/internal/` is part of the public surface. Those headers are pulled
 * in by the public ones that need them, and are not listed here on purpose.
 */

#include <ser/access.hpp>
#include <ser/archive/buffer.hpp>
#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/pointer_deny.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/macros.hpp>
#include <ser/pool/context.hpp>
#include <ser/serializer.hpp>
#include <ser/stream/header.hpp>
#include <ser/stream/read_write.hpp>
#include <ser/tags.hpp>
#include <ser/test.hpp>
#include <ser/traits.hpp>
#include <ser/type_config.hpp>

namespace ser {
	/** @brief The library's version, from `SER_VERSION`. */
	inline constexpr int VERSION = SER_VERSION;

} /* namespace ser */

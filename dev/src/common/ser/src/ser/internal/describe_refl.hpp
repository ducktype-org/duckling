#pragma once

#include <ser/config.hpp>

/*
 * Reserved for C++26 reflection (P2996 + expansion statements) and deliberately not
 * implemented: SER_HAS_REFLECTION needs both __cpp_impl_reflection and
 * __cpp_expansion_statements, and no compiler defines both today. If it ever fires, the
 * detection said yes with no implementation behind it, which must be loud.
 *
 * Guarded rather than unconditional, because the linter compiles every header on its own
 * and a bare #error would fail that for a file no build ever reaches.
 */
#if SER_HAS_REFLECTION
	#error \
		"ser: describe_refl.hpp is a placeholder - reflection support is not implemented. Build with structured bindings (do not define __cpp_impl_reflection by hand)."
#endif

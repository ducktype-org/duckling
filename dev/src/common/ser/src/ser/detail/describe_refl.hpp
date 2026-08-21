#pragma once

#include <ser/config.hpp>

// Reserved for C++26 reflection (P2996 + expansion statements). It is deliberately not
// implemented: SER_HAS_REFLECTION is the conjunction of __cpp_impl_reflection and
// __cpp_expansion_statements, and as of today no compiler defines both - GCC 16 has the
// expansion statements without the reflection.
//
// If this file is ever compiled, the detection said yes and there is no implementation
// behind it, which must be loud rather than silent.
// Guarded rather than unconditional: every header in this repository is also
// compiled on its own by the linter, and a bare #error would fail that for a
// file no build ever reaches.
#if SER_HAS_REFLECTION
	#error \
		"ser: describe_refl.hpp is a placeholder - reflection support is not implemented. Build with structured bindings (do not define __cpp_impl_reflection by hand)."
#endif

#pragma once

// Which implementation of "what are this type's fields" is in use.
//
// Not a fallback chain: structured bindings are the primary implementation, because
// C++26 reflection is available on no released compiler. describe_refl.hpp is a
// placeholder that fails loudly if this switch ever picks it by accident.
#include <ser/config.hpp>

#if SER_HAS_REFLECTION
	#include <ser/detail/describe_refl.hpp>
#else
	#include <ser/detail/describe_bind.hpp>
#endif

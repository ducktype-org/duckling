#include "all.hpp"

#include "internal/abstract_type_impl.hpp"

namespace tsh {
	void reset() { internal::getTypes().clear(); }
}

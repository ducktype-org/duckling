#include "all.hpp"
#include "internal/type_info_impl.hpp"

namespace tsh {
	void reset() { internal::getTypes().clear(); }
}

#include "../global_deinit.hpp"
#include <unicode/uclean.h>

namespace base::detail {
	ICUDeinit GlobalDeinit::icu_deinit;

	ICUDeinit::~ICUDeinit() { u_cleanup(); }
}

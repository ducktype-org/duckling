#include "icu_deinit.hpp"
#include <unicode/uclean.h>

namespace lexer::detail {
	ICUDeinit ICUDeinitManager::icu_deinit;

	ICUDeinit::~ICUDeinit() { u_cleanup(); }
}

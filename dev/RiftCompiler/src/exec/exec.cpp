#include "exec.hpp"
#include "builtin_values.hpp"
#include "ctv.hpp"
#include "helpers.hpp"
#include <exec/operators/builtinoperators.hpp>
#include <operations/operation.hpp>


namespace exec {
	void init() {
		static bool was_init = false;
		if(was_init)
			return;
		was_init = true;

		operation::init();
		initBuiltInOps();
	}
}

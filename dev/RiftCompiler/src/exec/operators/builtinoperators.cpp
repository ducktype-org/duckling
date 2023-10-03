#include "builtinoperators.hpp"

#include <exec/operators/arithmetic.hpp>
#include <exec/operators/operatorutils.hpp>
using namespace exec::operators;
using namespace ts;

namespace exec {
	BuiltInOpMap& getBuiltInOps() {
		static BuiltInOpMap built_in_ops;
		return built_in_ops;
	}

	// adds builtin operations to operations
	void initBuiltInOps() {
		NUM_BIN_ENTRIES_SIMPLE(Plus);
		NUM_BIN_ENTRIES_SIMPLE(Minus);
		NUM_BIN_ENTRIES_SIMPLE(Asterisk);
		NUM_BIN_ENTRIES_SIMPLE(Slash);
	}
}  // namespace exec

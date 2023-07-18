/**
 * @file arithmetic.hpp
 * @author actual implementation on arithmetic operations on ints
 *
 */

#pragma once

#include <exec/operators/operatorutils.hpp>

namespace exec::operators {
	NUM_BIN_SIMPLE_OPS(+, Plus)
	NUM_BIN_SIMPLE_OPS(-, Minus)
	NUM_BIN_SIMPLE_OPS(*, Asterisk)
	NUM_BIN_SIMPLE_OPS(/, Slash)
}

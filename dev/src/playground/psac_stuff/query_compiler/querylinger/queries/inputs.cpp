#include "inputs.hpp"

#include <query_framework/input_query/query_input_impl.hpp>

namespace ql {
	IMPLEMENT_QUERY_SIDE_INPUT(FnBodyInput);
	IMPLEMENT_QUERY_SIDE_INPUT(ConstInput);
	IMPLEMENT_QUERY_SIDE_INPUT(GlobalsInput);
}


#include "lookup.hpp"

#include <base/string_id.hpp>

namespace compiler::helios {

	// we need to add all the builtin symbols here,
	// likely via a query

	LookupResult lookupBuiltins(base::StrID name) {
		LookupResult output{};

		if (name == base::StrID("btn_test_symbol")) {
			// output.leaves.push_back(/* ... */);
		}

		return output;
	}

}

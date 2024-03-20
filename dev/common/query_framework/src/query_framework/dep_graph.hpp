#pragma once

#include "query_id.hpp"

namespace query {
	
	struct KeyHash {

	};

	struct NodeId {
		QueryID q_id;
		KeyHash hash;
	};

}

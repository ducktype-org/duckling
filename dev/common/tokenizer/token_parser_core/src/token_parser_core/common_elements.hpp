#pragma once

#include <base/string_id.hpp>
#include <base/optional.hpp>

namespace tpc {
	struct Identifier {
		base::StrId value;

		operator base::StrId() { return value; }
	};

	struct OptionalIdentifier {
		base::Optional<base::StrId> value;
	};
}

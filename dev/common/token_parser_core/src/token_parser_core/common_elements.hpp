#pragma once

#include <base/string_id.hpp>
#include <optional>

namespace tpc {
	struct Identifier {
		base::StrId value;

		operator base::StrId() { return value; }
	};

	struct OptionalIdentifier {
		std::optional<base::StrId> value;
	};
}

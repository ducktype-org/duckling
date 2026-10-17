#pragma once

#include "ids.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

namespace pg::ns {
	struct Namespace {
		base::StrID                               name;
		ns::NamespaceID                           id;
		base::Optional<NamespaceID>               parent_namespace;
		base::HashMap<base::StrID, sym::SymbolID> symbols;
	};
}

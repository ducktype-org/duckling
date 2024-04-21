#include "implicit_coercibility.hpp"

#include <map>
#include <set>

namespace ts {
	std::map<TypeInfo, std::set<TypeInfo>>& getUserDefinedImplicitCoercions() {
		static std::map<TypeInfo, std::set<TypeInfo>> userDefinedImplicitCoercions{};
		return userDefinedImplicitCoercions;
	}
}

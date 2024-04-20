#include "implicit_coercibility.hpp"
#include "type_desc.tcpp"

#include <map>
#include <set>

namespace ts {
	std::map<TypeInfo, std::set<TypeInfo>>& getUserDefinedImplicitCoercions() {
		static std::map<TypeInfo, std::set<TypeInfo>> userDefinedImplicitCoercions{};
		return userDefinedImplicitCoercions;
	}

	void addUserDefinedImplicitCoercion(const TypeInfo& source, const TypeInfo& target) {
		getUserDefinedImplicitCoercions()[source].insert(target);
	}

	bool isImplicitlyCoercible(const TypeInfo& source, const TypeInfo& target) {
		// Implicit coercion is possible when either the user defined it,
		// or if the type kind knows to implicitly coerce to the target type,
		// often of the same kind, e.g. integer promotion or upwards a class hierarchy.
		return (
			(getUserDefinedImplicitCoercions().contains(source)
		     && getUserDefinedImplicitCoercions()[source].contains(target))
			|| source.isInfoImplicitlyCoercible(target)
		);
	}

	bool isImplicitlyCoercible(const TypeDesc<>& source, const TypeDesc<>& target) {
		// Implicit coercibility between TypeDescs is decided by the TypeDesc we are coercing from.
		return source.isDescImplicitlyCoercible(target);
	}
}

#include "implicit_coercibility.hpp"
#include "type_desc.tcpp"

#include <map>
#include <set>

namespace ts {
	std::map<TypeInfo, std::set<TypeInfo>> userDefinedImplicitCoercions{};

	void addUserDefinedImplicitCoercion(const TypeInfo &from, const TypeInfo &to) {
		userDefinedImplicitCoercions[from].insert(to);
	}

	bool isImplicitlyCoercible(const TypeInfo &from, const TypeInfo &to) {
		// Implicit coercion is possible when either the user defined it,
		// or if the type kind knows to implicitly coerce to the target type,
		// often of the same kind, e.g. integer promotion or upwards a class hierarchy.
		return (
			(userDefinedImplicitCoercions.contains(from)
		     && userDefinedImplicitCoercions[from].contains(to))
			|| from.isInfoImplicitlyCoercible(to)
		);
	}

	bool isImplicitlyCoercible(const TypeDesc<> &from, const TypeDesc<> &to) {
		// Implicit coercibility between TypeDescs is decided by the TypeDesc we are coercing from.
		return from.isDescImplicitlyCoercible(to);
	}
}

#include "value_source.hpp"

#include "../types.hpp"
#include "../value_category.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::tsh::coercions {
	ValueSource ValueSource::dereferenced() const {
		CORE_ASSERT(
			not isBuiltOutOfParts(), "A value built out of parts should never be behind a reference."
		);

		return singleValueSource({
			value.getSymbolType().getPointeeSymbolType(),
			ValueCategory(PrimaryCategory::Dereferenced),
		});
	}

	ValueSource ValueSource::partAt(const usize index) const {
		const TupleAbstractType          tuple{ value.getType() };
		const std::vector<SymbolType<>>& components = tuple.getComponents();
		CORE_ASSERT(index < components.size(), "ValueSource part out of bounds");

		// A part of a value that already exists came from wherever the value came from.
		if (not isBuiltOutOfParts())
			return singleValueSource({ components[index], value.getValueCategory() });

		CORE_ASSERT(
			parts.size() == components.size(),
			"A value built out of parts should be built out of one per component of its type."
		);
		CORE_ASSERT(
			parts[index].getValue().getSymbolType() == components[index],
			"The part given for a value has to be the part the type lists in that place."
		);

		return parts[index];
	}
}

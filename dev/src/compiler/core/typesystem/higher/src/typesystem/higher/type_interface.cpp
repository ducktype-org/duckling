#include "type_interface.hpp"

#include "queries.hpp"

#include <helios/symbols/simple.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context.hpp>

namespace tsh {
	namespace {
		base::Map<base::StrID, std::vector<InterfaceElement>> groupElementsByName(
			const std::vector<InterfaceElement>& elements
		) {
			base::Map<base::StrID, std::vector<InterfaceElement>> result{};
			for (const InterfaceElement& element: elements) {
				base::StrID name = compiler::helios::name(element.getSymbol());
				if (!result.contains(name)) result.put(name, {});
				result.at(name).push_back(element);
			}
			return result;
		}
	}

	TypeInterface::TypeInterface(const std::vector<InterfaceElement>& elements):
		  elements(elements),
		  elements_by_name(groupElementsByName(elements)) {}

	const base::Map<base::StrID, std::vector<InterfaceElement>>& TypeInterface::getElementsByName(
	) const {
		return elements_by_name;
	}

	const std::vector<InterfaceElement>& TypeInterface::getElementsWithName(const base::StrID name
	) const {
		static constexpr std::vector<InterfaceElement> EMPTY{};
		if (elements_by_name.contains(name)) return elements_by_name.at(name);
		return EMPTY;
	}

	SymbolType<> InterfaceElement::getType(query::Context& ctx) const {
		if (isField()) return getResultType();

		// @TODO: #1396 Add .is_mutable and .pure when additional method specifiers are supported.
		std::vector<SymbolType<>> all_parameter_types{};
		// @note: The first parameter is the implicit self parameter. It might change to
		// being specified in the method declaration.
		all_parameter_types.emplace_back(source, ReferenceKind::Ref, Mutability::Mutable);
		for (const auto& par: parameters.value()) all_parameter_types.push_back(par.type);
		return SymbolType{
			ctx.query<QueryFunctionType>({
				.parameter_types = all_parameter_types,
				.result_type     = result_type,
				.pure            = false,
				.free            = false,
			}),
			ReferenceKind::Direct,
			Mutability::Immutable,
		};
	}

	using ResolutionResult = TypeInterface::ResolutionResult;
	using NamedArgument    = TypeInterface::NamedArgument;
	using Parameter        = InterfaceElement::Parameter;

	// ResolutionResult TypeInterface::resolve(const base::StrID name, query::Context&) const {
	// 	const std::vector<InterfaceElement>& elements_matching_name = getElementsWithName(name);
	// 	if (elements_matching_name.empty()) return NoMatch{ {} };
	// 	if (elements_matching_name.size() == 1)
	// 		return SingleMatch{ .best_match=*elements_matching_name.begin(), .alternative_matches={}, .non_matches={} };
	// 	return AmbiguousMatch{ .conflicting_matches=elements_matching_name, .alternative_matches={}, .non_matches={} };
	// }

}

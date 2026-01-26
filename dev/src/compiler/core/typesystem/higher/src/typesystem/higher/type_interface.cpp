#include "type_interface.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::tsh {
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
		return ctx.query<compiler::helios::QueryTypeOfSymbol>(symbol)->valueOrThrow();
	}
}

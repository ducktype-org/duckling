#include "type_interface.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/symbols/symbols.hpp>

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

	base::OkBad TypeInterface::checkForDuplicates() const {
		std::set<InterfaceElement> element_set;
		element_set.insert(elements.begin(), elements.end());
		return element_set.size() == elements.size() ? base::OK : base::BAD;
	}

	TypeInterface::TypeInterface(const std::vector<InterfaceElement>& elements):
		  elements(elements),
		  elements_by_name(groupElementsByName(elements)) {
		CORE_ASSERT(checkForDuplicates().isOk(), "Duplicate elements in type interface");
	}

	TypeInterface TypeInterface::combine(const CRef<TypeInterface> other) const {
		std::set<InterfaceElement>    my_element_set;
		std::vector<InterfaceElement> new_elements = elements;
		my_element_set.insert(elements.begin(), elements.end());
		for (auto& other_element: other->elements)
			if (!my_element_set.contains(other_element)) new_elements.push_back(other_element);
		return TypeInterface(new_elements);
	}

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

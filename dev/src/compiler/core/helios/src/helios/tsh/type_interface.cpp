#include "type_interface.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::tsh {
	namespace {
		base::HashMap<base::StrID, std::vector<InterfaceElement>> groupElementsByName(
			const std::vector<InterfaceElement>& elements
		) {
			base::HashMap<base::StrID, std::vector<InterfaceElement>> result{};
			for (const InterfaceElement& element: elements) {
				base::StrID name = compiler::helios::name(element.getSymbol());
				if (!result.contains(name)) result.put(name, {});
				result.at(name).push_back(element);
			}
			return result;
		}

		base::HashMap<MemberSpecialKind, InterfaceElement> groupElementBySpecialKind(
			const std::vector<InterfaceElement>& elements
		) {
			base::HashMap<MemberSpecialKind, InterfaceElement> result{};
			for (const InterfaceElement& element: elements) {
				auto special_kind = element.specialKind();
				if (special_kind == MemberSpecialKind::None) continue;
				result.put(special_kind, element);
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
		  elements_by_name(groupElementsByName(elements)),
		  element_by_special_kind(groupElementBySpecialKind(elements)) {
		CORE_ASSERT(checkForDuplicates().isOk(), "Duplicate elements in type interface");
	}

	base::Optional<CRef<InterfaceElement>> TypeInterface::getSpecialElement(
		const MemberSpecialKind special
	) const {
		if (not element_by_special_kind.contains(special)) return {};
		return CRef(&element_by_special_kind.at(special));
	}

	const base::HashMap<base::StrID, std::vector<InterfaceElement>>& TypeInterface::getElementsByName(
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

	void TypeInterfaceBuilder::push(
		compiler::helios::SymID                symbol,
		InterfaceElement::InterfaceElementKind kind,
		MemberVisibility                       visibility,
		MemberSpecialKind                      special
	) {
		interface_elements.emplace_back(symbol, owner, declaration_order, kind, visibility, special);
		declaration_order++;
	}

	TypeInterface TypeInterfaceBuilder::build() const { return TypeInterface(interface_elements); }

}

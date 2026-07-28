#include "inherited_type_with_specifiers.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

namespace compiler::helios {

	std::vector<std::string> collectTypeSpecifiers(const tsh::SymbolType<>& symbol_type) {
		std::vector<std::string> specifiers;

		if (symbol_type.getUniqueness() == tsh::Uniqueness::Unique)
			specifiers.emplace_back("unique");
		if (symbol_type.getLeakage() == tsh::Leakage::Leaking) specifiers.emplace_back("leaking");
		if (symbol_type.getMutability() == tsh::Mutability::Immutable)
			specifiers.emplace_back("const");
		switch (symbol_type.getRefKind()) {
		case tsh::ReferenceKind::Direct:
			break;
		case tsh::ReferenceKind::Box:
			specifiers.emplace_back("box");
			break;
		case tsh::ReferenceKind::Ref:
			specifiers.emplace_back("ref");
			break;
		}

		return specifiers;
	}

	InheritedTypeWithSpecifiersError::InheritedTypeWithSpecifiersError(
		query::Context&         ctx,
		dia_int::StablePosition source_position,
		std::string             class_name,
		InheritanceKind         inheritance_kind,
		tsh::SymbolType<>       inherited_type
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		std::string specifiers;
		for (const auto& specifier: collectTypeSpecifiers(inherited_type)) {
			if (!specifiers.empty()) specifiers += ", ";
			specifiers += '`' + specifier + '`';
		}

		addArgument<dia_int::TextArgument>("class_name", std::move(class_name));
		addArgument<dia_int::TextArgument>(
			"inheritance_kind",
			inheritance_kind == InheritanceKind::ExtendedClass ? "extended class"
															   : "implemented interface"
		);
		addArgument<dia_int::InteractiveArgument>(
			"inherited_type", makeBox<InteractiveType>(ctx, inherited_type)
		);
		addArgument<dia_int::TextArgument>("specifiers", std::move(specifiers));
	}

}

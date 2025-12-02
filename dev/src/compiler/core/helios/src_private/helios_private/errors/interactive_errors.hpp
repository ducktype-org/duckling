#pragma once

#include <diagnostic_interactive/usage.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios::errors {
	class InteractiveType: public dia_int::InteractiveElement {
		tsh::SymbolType<>                             symbol_type;
		base::Optional<pst::Access<pst::ExprElement>> pst_expr;

		Box<dia_int::dia_args::Component> getValue(dia_int::MessageBase& msg) final;

	public:
		InteractiveType(
			tsh::SymbolType<>                             symbol_type,
			base::Optional<pst::Access<pst::ExprElement>> pst_expr = {}
		);
	};

	class VariableElement {};

	class IncompatibleTypesError: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "incompatible_types" };
		}

	public:
		IncompatibleTypesError(
			dia::SourcePosition source_position,
			InteractiveType     expected_type,
			InteractiveType     actual_type
		);
	};
}

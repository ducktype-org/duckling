#pragma once

#include "diagnostic_interactive/core/diagnostic_file.hpp"
#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/elements/hierarchy/expr_holders.hpp"
#include "frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp"
#include "frontend/pst_parser/elements/hierarchy/statements/alias.hpp"
#include "helios_private/lookup/interface.hpp"
#include "helios_private/scopes/scopes.hpp"
#include "helios_private/symbols/symbol_data.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/usage.hpp>
#include <helios/symbols/simple.hpp>

#include "diagnostic/source_position.hpp"
#include "query_framework/context.hpp"
#include <query_framework/utils/with_context_do.hpp>

#include <functional>
#include <utility>
#include <vector>

namespace compiler::helios::errors {
	using namespace dia_int;


	inline std::string getStr(dia::SourcePosition pos) {
		return pos.getSource()->getCharRange(pos.getStart(), pos.getEnd() + 1).stdString();
	}

	class InteractiveType: public InteractiveElement {
		tsh::SymbolType<>                             symbol_type;
		base::Optional<pst::Access<pst::ExprElement>> pst_expr;

		Box<dia_file::Component> build(MessageBase& msg) final;

	public:
		InteractiveType(
			tsh::SymbolType<> symbol_type, base::Optional<pst::Access<pst::ExprElement>> pst_expr
		);
	};

	class VariableElement {};

	class IncompatibleTypesError: public MessageWithCodeFragmentAndCause {
		Metadata getMetadata() const final {
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

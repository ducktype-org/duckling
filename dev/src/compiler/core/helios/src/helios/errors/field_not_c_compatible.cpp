#include "field_not_c_compatible.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

namespace compiler::helios {

	FieldNotCCompatibleError::FieldNotCCompatibleError(
		query::Context&         ctx,
		dia_int::StablePosition source_position,
		std::string             field_name,
		tsh::SymbolType<>       field_type,
		std::string             reason
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia_int::TextArgument>("field_name", std::move(field_name));
		addArgument<dia_int::InteractiveArgument>(
			"field_type", makeBox<InteractiveType>(ctx, field_type)
		);
		addArgument<dia_int::TextArgument>("reason", std::move(reason));
	}

}

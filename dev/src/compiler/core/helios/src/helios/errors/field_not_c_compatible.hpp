#pragma once

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context_fd.hpp>

#include <string>

namespace compiler::helios {

	/**
	 * @brief Error reported when a field of an `extern("C")` class has a type
	 * that cannot be represented in the C ABI.
	 *
	 * The `reason` argument carries a short human-readable explanation of why
	 * the field's type was rejected; the converter responsible for mapping a
	 * Duckling type to the C ABI produces it.
	 */
	class FieldNotCCompatibleError final: public dia_int::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "field_not_c_compatible" };
		}

	public:
		FieldNotCCompatibleError(
			query::Context&         ctx,
			dia_int::StablePosition source_position,
			std::string             field_name,
			tsh::SymbolType<>       field_type,
			std::string             reason
		);
	};

}

#pragma once

#include <helios/tsh/symbol_type.hpp>

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>
#include <query_framework/context/context_fd.hpp>

#include <string>

namespace compiler::helios {

	/**
	 * @brief Error reported when a field of an `extern("C")` class has a type
	 * that cannot be represented in the C ABI.
	 *
	 * The `reason` argument, produced by the C-ABI converter, carries a short
	 *  explanation of why the field's type was rejected.
	 */
	class FieldNotCCompatibleError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "field_not_c_compatible" };
		}

	public:
		FieldNotCCompatibleError(
			query::Context&     ctx,
			dia::StablePosition source_position,
			std::string         field_name,
			tsh::SymbolType<>   field_type,
			std::string         reason
		);
	};

}

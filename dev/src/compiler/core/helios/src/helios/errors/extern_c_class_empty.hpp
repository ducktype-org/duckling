#pragma once

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>

#include <string>

namespace compiler::helios {

	/**
	 * @brief Error reported when an `extern("C")` class has no fields.
	 *
	 * Empty structs are not legal in C, so such a class has no valid C-ABI
	 * layout.
	 */
	class ExternCClassEmptyError final: public dia_int::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "extern_c_class_empty" };
		}

	public:
		ExternCClassEmptyError(dia_int::StablePosition source_position, std::string class_name);
	};

}

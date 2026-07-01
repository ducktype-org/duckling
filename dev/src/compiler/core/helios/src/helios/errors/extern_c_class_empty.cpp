#include "extern_c_class_empty.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>

namespace compiler::helios {

	ExternCClassEmptyError::ExternCClassEmptyError(
		dia_int::StablePosition source_position, std::string class_name
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia_int::TextArgument>("class_name", std::move(class_name));
	}

}

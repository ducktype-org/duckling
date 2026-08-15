#include "extern_c_class_empty.hpp"

#include <diagnostic/core/diagnostic_arguments.hpp>

namespace compiler::helios {

	ExternCClassEmptyError::ExternCClassEmptyError(
		dia::StablePosition source_position, std::string class_name
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia::TextArgument>("class_name", std::move(class_name));
	}

}

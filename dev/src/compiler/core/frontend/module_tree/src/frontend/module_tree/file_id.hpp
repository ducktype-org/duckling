#pragma once

#include <base/strongly_typed_id.hpp>

#include <functional>  // IWYU pragma: export

namespace compiler::frontend {

	/**
	 * @brief Structure holding FileID within SourceFile
	 */
	STRONG_TYPEDEF_ID(FileID);
}

ID_STD_HASH(::compiler::frontend::FileID);

#pragma once
#include <diagnostic/source_position.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace dia {
	struct SerializationParams {
		bool include_code = true;  // This just an example flag. I don't know if it makes sense. It
		                           // was just the first thing that came to my mind.
		bool   include_symbols    = true;
		bool   include_types      = true;
		size_t default_code_lines = 1;
	};

	using pointer_message = std::pair<std::string, dia::SourcePosition>;
}

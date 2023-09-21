#pragma once

#include "status.hpp"
#include <services_data/type_metadata/type.hpp>
#include <memory_data/block.hpp>

namespace vm::api {
	namespace response {
		struct Empty {};

		struct Output {
			std::string output;
			JS_OBJ(output);
		};
		
		struct Block {
			base::RawView data;
			JS_OBJ(data);
		};
	}

	using Response = std::variant<
		VCPUStatus,
		response::Output,
		response::Block,
		TypeCRef,
		response::Empty
	>;
}
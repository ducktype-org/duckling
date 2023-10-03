#pragma once

#include <memory_data/block.hpp>
#include <services_data/type_metadata/type.hpp>

#include "status.hpp"

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
	}  // namespace response

	using Response
		= std::variant<VCPUStatus, response::Output, response::Block, TypeCRef, response::Empty>;
}  // namespace vm::api

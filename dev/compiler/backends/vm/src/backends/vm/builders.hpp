#pragma once


#include "../../../../../../VM/src/preprocessor/parser/types_of_data.hpp"
#include <base/string_id.hpp>
#include "instructions.hpp"

namespace compiler::backend_vm {
	class BlockBuilder {

    };
	class FunctionBuilder {

    };
    class ModuleBuilder {
        public:
        std::vector<vm::TypeOfData> types;
    };
}

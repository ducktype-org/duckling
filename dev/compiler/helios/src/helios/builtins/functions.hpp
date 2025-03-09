#pragma once

#include <typesystem/higher/types.hpp>

namespace compiler::helios::builtin {

    struct BuiltinFunctionData final {
        tsh::FunctionAbstractType type;
        BuiltinFunctionData(tsh::FunctionAbstractType type): type(type) {}
    };

}
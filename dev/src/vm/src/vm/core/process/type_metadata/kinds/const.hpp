#pragma once

#include "../definitions.hpp"

namespace vm::kind {
    struct Const {
        TypeRef inner_type;
    };
}
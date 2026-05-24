#include <abi/type_system/type.hpp>

// The ABI type system is currently header-only: every type and helper is a
// plain aggregate or inline function. This translation unit exists so that the
// CMake target has at least one source file.

namespace abi::type_system {}

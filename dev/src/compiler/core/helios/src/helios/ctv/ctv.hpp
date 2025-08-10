#pragma once

#include <vm/core/thread/vmvalue.hpp>

#include <typesystem/higher/symbol_type.hpp>

#include <base/box.hpp>

#include <variant>

namespace compiler::helios {
	struct VmHeldValue {
		tsh::SymbolType<> type;
		MBox<vm::VmValue> value{};
	};

	using CompileTimeValue = std::variant<i64, i32, u64, bool, tsh::SymbolType<>, VmHeldValue>;
	using CTV              = CompileTimeValue;
}

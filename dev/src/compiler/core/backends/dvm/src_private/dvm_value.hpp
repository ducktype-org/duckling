#pragma once

#include <string_id/string_id.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <utility>

namespace compiler::backend_vm::internal {
	struct DVMLocal {
		base::StrID          name;
		vm::code::TypeOfData type;

		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};

	struct DVMGlobal {
		base::StrID          name;
		vm::code::TypeOfData type;

		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};

	struct DVMImmediate {
		DVMImmediate(u64 value);
		DVMImmediate(i64 value);
		DVMImmediate(i32 value);
		DVMImmediate(u32 value);
		DVMImmediate(bool value);
		DVMImmediate(float value);
		DVMImmediate(double value);

		// All values are represented as u64, so e.g. a float is bit-casted to u64.
		u64 value{};

		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};

	struct DVMLabel {
		base::StrID name;

		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};

	struct DVMFunctionName {
		base::StrID name;

		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};

	class DVMValue {
		using StoredValueVariant
			= std::variant<DVMLocal, DVMGlobal, DVMImmediate, DVMLabel, DVMFunctionName>;

		StoredValueVariant stored_value;

	public:
		DVMValue(StoredValueVariant value): stored_value(std::move(value)) {}

		operator vm::opargs::OpCodeArg() const;
		[[nodiscard]] vm::opargs::OpCodeArg asArgument() const;
	};
}

#pragma once

#include "flag_context.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/valid_function.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>

namespace vm {
	class SafeVMValue;
}

namespace vm::code::detail {

	struct Normal {};

	struct Expr {
		base::CRef<std::deque<Box<SafeVMValue>>> vm_values;
		const Frame*                             call_stack_base;
		usize                                    call_stack_size;

		[[nodiscard]] base::Optional<vm::loader::ValidFuncPosition> getUpcomingHighPosition(
			usize frame_idx
		) const {
			if (frame_idx >= call_stack_size) return std::nullopt;
			const Frame& frame = call_stack_base[frame_idx];

			auto& func          = *frame.current_function;
			auto  low_instr_idx = static_cast<u64>(frame.instr - func.getBc().data());

			auto fat_pos = func.mapLowVMProgramPositionToCodeCollectionPosition(low_instr_idx);
			if (!fat_pos) return std::nullopt;

			return loader::ValidFuncPosition(fat_pos->instruction_index, func.getHighFunc());
		}
	};

	struct StartFunction {};

	using ValidationMode = std::variant<Normal, Expr, StartFunction>;

	/**
	 * @brief Performs function code validation in the given context and extracts reachable code.
	 */
	valid_function::ValidFunction validateAndExtractReachableCode(
		const valid_type::ValidTypeMap&                  types,
		const ObjIdNameMap<GlobalData>&                  globals_map,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_functions,
		const FlagContext&                               flag_context,
		const ObjIdNameMap<FFIFunction>&                 ffi_functions,
		const Function&                                  function,
		ValidationMode                                   mode = Normal{}
	);
}

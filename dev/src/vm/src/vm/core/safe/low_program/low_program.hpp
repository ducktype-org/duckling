/**
 * @file low_program.hpp
 */
#pragma once

#include "cfg/cf_graph.hpp"
#include "instruction.hpp"

#include <ffi.h>

#include <base/pointers/box.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/const_value.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#ifdef ENABLE_JIT
	#include <vm/core/jit/jit_compiler.hpp>
#endif

#include <variant>

namespace vm::loader::compiler::safe {
	class SafeCompiler;
}

namespace vm::low {
	using MicroBytecode = std::vector<MicroInstruction>;

	/**
	 * @brief Micro bytecode representation of function data.
	 */
	struct LowFuncData final {
		base::StrID name;
		usize       id;
#ifdef ENABLE_JIT
		/// Offset of the JIT function entrypoint in the bytecode (guarded by a `nop` in `bc`).
		usize jit_func_entrypoint_offset;
		/// Per-function JIT state (CFGs, compilation thresholds, compiled code).
		/// Default-constructed empty; filled in by the compiler only when the JIT is enabled.
		/// Mutable because opcode functions only hold a const reference to the function
		/// (same precedent as LowFFIFunction::cif).
		mutable jit::JitFuncData jit_data{};
#endif
		MicroBytecode bc;
		// the copy of original bytecode. Always has the same length as bc
		// kept for debugging & JIT purposes
		MicroBytecode orig_bc;

		/// The maximum size of the local variables on stack required by the function frame.
		usize local_stack_size;
		/// The maximum count of local variable slots required by the function frame.
		usize local_slot_count;

		usize arg_size;
		// The total summed size of all return values.
		usize                 ret_size;
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_types;

		/**
		 * @brief Range of instructions
		 * @note Represents inclusive-exclusive range [`begin`, `end`)
		 */
		struct InstructionRange final {
			usize begin, end;
			auto  operator<=>(const InstructionRange&) const = default;

			[[nodiscard]] bool contains(usize index) const { return begin <= index && index < end; }
		};

		/**
		 * @brief Mapping of fatbytecode instruction indexes to microbytecode instruction indexes
		 * ranges.
		 * @note Vector indexes correspond to FatBytecode instruction indexes
		 */
		std::vector<InstructionRange> instruction_mapping;

		/**
		 * @brief method for setting the breakpoint in microbytecode
		 * @note this is a fundamental property of the microbytecode representation
		 */
		std::expected<void, std::string> setBreakpoint(usize idx, bool enable) {
			CORE_ASSERT(orig_bc.size() == bc.size(), "any edits made to bc cannot change length");

			if (instruction_mapping.size() <= idx)
				return std::unexpected{ "setBreakpoint: Function too short" };

			usize micro_instruction_index = instruction_mapping[idx].begin;

			if (bc.size() <= micro_instruction_index)
				return std::unexpected("setBreakpoint: No code after breakpoint");

			auto& instr = bc.at(micro_instruction_index);

			if (enable)
				instr = makeLowInstruction(low::MicroOpcode::breakpoint, instr.arg0, instr.arg1);
			else
				instr = orig_bc.at(micro_instruction_index);

			return {};
		}

#ifdef ENABLE_JIT
		/**
		 * @brief Replaces opcode at the provided bytecode index with the provided opcode.
		 * @returns Original opcode from the provided location on success and `nullopt` if the
		 * location does not exist.
		 */
		base::Optional<MicroOpcode> replaceOpcode(usize instruction_index, MicroOpcode opcode) {
			if (bc.size() <= instruction_index) return std::nullopt;

			auto&       instruction     = bc[instruction_index];
			MicroOpcode original_opcode = getInstructionOpcode(instruction);

			instruction = makeLowInstruction(opcode, instruction.arg0, instruction.arg1);

			return original_opcode;
		}
#endif
	};

	struct LowCodePosition final {
		CRef<LowFuncData> function;
		usize             instruction_index;
	};

	/**
	 * @brief Initialization strategy for a global variable.
	 * Either initialized via constructor/destructor functions or via a constant initial value.
	 */
	struct GlobalCtorDtor final {
		base::Optional<base::StrID> ctor_name;
		base::Optional<base::StrID> dtor_name;
	};

	struct GlobalInitialValue final {
		code::ConstantValue value;
	};

	using GlobalInit = std::variant<GlobalCtorDtor, GlobalInitialValue>;

	/**
	 * @brief Micro bytecode representation of global data.
	 */
	struct LowGlobalData final {
		/// Type
		TypeCRef type;

		/// Initialization strategy: either ctor/dtor or constant initial value.
		GlobalInit init;

		/// The offset of the global variable's data in the global buffer.
		usize global_buffer_offset;

		/// The index of the global block ref in the global block array.
		usize global_block_idx;
	};

	/**
	 * @brief Micro bytecode representation of an extern C function.
	 */
	struct LowExternCFunction final {
		base::StrID name;
		void (*function_pointer)(byte*, byte*) = nullptr;
		usize                 parameter_size_sum;
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_types;
	};

	/**
	 * @brief Micro bytecode representation of an FFI function, ready to be called via libffi.
	 * @note `cif` points into `ffi_arg_types` and `struct_types`, so instances must not be
	 * mutated after `ffi_prep_cif` was performed on them (moving the whole object is fine, as
	 * the pointed-to storage lives on the heap).
	 */
	struct LowFFIFunction final {
		base::StrID name;

		/// Native symbol address resolved from one of the loaded object files.
		void (*symbol)() = nullptr;

		/// Mutable because `ffi_call` takes a non-const pointer, although it never modifies it.
		mutable ffi_cif cif;
		/// Argument types array `cif` points into.
		std::vector<ffi_type*> ffi_arg_types;
		/// Owning storage for libffi struct type descriptors used in the signature.
		std::vector<Box<ffi_type>> struct_types;
		/// Owning storage for the element arrays of `struct_types` entries.
		std::vector<Box<std::vector<ffi_type*>>> struct_elements;

		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_types;
	};

	class ILowVMProgram {
	public:
		[[nodiscard]]
		virtual const TypeMetadata& getTypes() const
			= 0;

		[[nodiscard]]
		virtual const ObjIdNameMap<LowFuncData, usize>& getFunctions() const
			= 0;

		[[nodiscard]]
		virtual const StableObjIdNameMap<LowExternCFunction>& getExternCFunctions() const
			= 0;

		[[nodiscard]]
		virtual const StableObjIdNameMap<LowFFIFunction>& getFFIFunctions() const
			= 0;

		[[nodiscard]]
		virtual const ObjIdNameMap<LowGlobalData, GlobalDataID>& getGlobals() const
			= 0;

		[[nodiscard]]
		virtual const base::HashMap<u64, base::StrID>& getMethodNamePool() const
			= 0;

		/**
		 * @brief Helper structure with the configuration for the global buffer in the program.
		 * Global buffer is the contiguous memory area where the data of global variables is stored.
		 *
		 * Used mainly by the VMProcess to determine the amount of memory to allocate for the globals.
		 */
		struct GlobalBufferConfig final {
			Bytes buffer_size;   /// The sum of sizes of all the global variables in the program.
			usize global_count;  /// The count of global variables in the program
		};

		/**
		 * @brief Get the global buffer configuration.
		 */
		[[nodiscard]]
		virtual GlobalBufferConfig getGlobalBufferConfig() const
			= 0;

		virtual ~ILowVMProgram() = default;
	};

	/**
	 * @brief Representation of the micro bytecode program which the VM runs.
	 * This is the final form of bytecode produced by the loader module which is executable by
	 * `VMThread`.
	 *
	 * @note This structure can only be created by the compiler.
	 * @note The program represented by this structure is always valid as it was verified in the
	 * loading stage.
	 *
	 * @note The order of functions in the `ObjIdNameMap<LowFuncData, usize>` is important, as the
	 * `ID` of the function used when performing function calls is the index in the `std::vector`
	 * (used internally in `ObjIdNameMap`). Similar holds for `ObjIdNameMap<LowGlobalData,
	 * GlobalDataID>` - global data.
	 */
	class LowVMProgram final: public ILowVMProgram {
	public:
		friend class vm::loader::compiler::safe::SafeCompiler;

		const TypeMetadata& getTypes() const override { return *types; }

		const ObjIdNameMap<LowFuncData, usize>& getFunctions() const override { return functions; }

		const StableObjIdNameMap<LowExternCFunction>& getExternCFunctions() const override {
			return extern_c_functions;
		}

		const StableObjIdNameMap<LowFFIFunction>& getFFIFunctions() const override {
			return ffi_functions;
		}

		const ObjIdNameMap<LowGlobalData, GlobalDataID>& getGlobals() const override {
			return global_data;
		}

		const base::HashMap<u64, base::StrID>& getMethodNamePool() const override {
			return method_name_pool;
		}

		GlobalBufferConfig getGlobalBufferConfig() const override {
			return { .buffer_size = global_buffer_size, .global_count = global_count };
		}

	private:
		LowVMProgram()                                  = default;
		Box<TypeMetadata>                         types = makeBox<TypeMetadata>();
		ObjIdNameMap<LowFuncData, usize>          functions{};
		StableObjIdNameMap<LowExternCFunction>    extern_c_functions{};
		StableObjIdNameMap<LowFFIFunction>        ffi_functions{};
		ObjIdNameMap<LowGlobalData, GlobalDataID> global_data{};
		Bytes                                     global_buffer_size = Bytes(0);
		usize                                     global_count       = 0;

		// Contains all method names in the program. It's used by the executor to determine the
		// names of called functions.
		// @TODO: #2685 This is redundant, u64 is as fast as base::StrID.
		base::HashMap<u64, base::StrID> method_name_pool{};
	};
}

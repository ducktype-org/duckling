/**
 * @file low_program.hpp
 */
#pragma once

#include "instruction.hpp"

#include <base/pointers/box.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::loader::compiler {
	class Compiler;
}

namespace vm::low {
	using MicroBytecode = std::vector<MicroInstruction>;

	struct LowCodePosition {
		u64 function_id;
		u64 instruction_index;
	};

	/**
	 * @brief Micro bytecode representation of function data.
	 */
	struct LowFuncData {
		base::StrID   name;
		MicroBytecode bc;

		/// The maximum size of the local variables on stack required by the function frame.
		usize local_stack_size;
		/// The maximum count of blocks required by the function frame.
		usize local_block_count;

		usize arg_size;
		// The total summed size of all return values.
		usize                 ret_size;
		std::vector<TypeCRef> parameters;
		std::vector<TypeCRef> result_types;

		/**
		 * @brief Range of instructions
		 * @note Represents inclusive-exclusive range [`begin`, `end`)
		 */
		struct InstructionRange {
			usize begin, end;
			auto  operator<=>(const InstructionRange&) const = default;
		};

		/**
		 * @brief Mapping of fatbytecode instruction indices to microbytecode instruction indice
		 * ranges.
		 * @note Vector indexes correspond to FatBytecode instruction indexes
		 */
		base::Optional<std::vector<InstructionRange>> instruction_mapping{};
	};

	/**
	 * @brief Micro bytecode representation of global data.
	 */
	struct LowGlobalData {
		/// Type
		TypeCRef type;

		/// Optional constructor name.
		base::Optional<base::StrID> ctor_name;

		/// Optional destructor name.
		base::Optional<base::StrID> dtor_name;

		/// The offset of the global variable's data in the global buffer.
		usize global_buffer_offset;

		/// The index of the global block ref in the global block array.
		usize global_block_idx;
	};

	/**
	 * @brief Micro bytecode representation of an extern C function.
	 */
	struct LowExternCFunction {
		base::StrID name;
		void (*function_pointer)(std::byte*, std::byte*) = nullptr;
		usize                 parameter_size_sum;
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
		struct GlobalBufferConfig {
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
		friend class vm::loader::compiler::Compiler;

		const TypeMetadata& getTypes() const override { return *types; }

		const ObjIdNameMap<LowFuncData, usize>& getFunctions() const override { return functions; }

		const StableObjIdNameMap<LowExternCFunction>& getExternCFunctions() const override {
			return extern_c_functions;
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
		ObjIdNameMap<LowGlobalData, GlobalDataID> global_data{};
		Bytes                                     global_buffer_size = Bytes(0);
		usize                                     global_count       = 0;

		// Contains all method names in the program. It's used by the executor to determine the
		// names of called functions.
		base::HashMap<u64, base::StrID> method_name_pool{};
	};

	/**
	 * @brief Overlay over `LowVMProgram` with its own and therefore modifiable copy of functions.
	 * @note Only the functions can be copied and modified, the types and extern C
	 * functions are shared with the original program and are not modifiable through this structure
	 * because of the way instruction arguments are currently being lowered - they contain direct
	 * pointers to types.
	 *
	 * @note Needs updating via `selfUpdate()` to make new functions visible.
	 * @note Program with current everything except functions is still a valid program.
	 *
	 * @note Lookup in `getMethodNamePool()` may give false-positive if program is not updated.
	 */
	class LowVMProgramCopy final: public ILowVMProgram {
		CRef<LowVMProgram>               original_program;
		ObjIdNameMap<LowFuncData, usize> functions{};

	public:
		const TypeMetadata& getTypes() const override { return original_program->getTypes(); }

		const ObjIdNameMap<LowFuncData, usize>& getFunctions() const override { return functions; }

		const StableObjIdNameMap<LowExternCFunction>& getExternCFunctions() const override {
			return original_program->getExternCFunctions();
		}

		const ObjIdNameMap<LowGlobalData, GlobalDataID>& getGlobals() const override {
			return original_program->getGlobals();
		}

		const base::HashMap<u64, base::StrID>& getMethodNamePool() const override {
			return original_program->getMethodNamePool();
		}

		GlobalBufferConfig getGlobalBufferConfig() const override {
			return original_program->getGlobalBufferConfig();
		}

		CRef<LowVMProgram> getOriginalProgram() const { return original_program; }

		LowVMProgramCopy(CRef<LowVMProgram> original_program): original_program(original_program) {}

		/**
		 * @brief Updates itself to reflect original `LowVMProgram` state
		 */
		LowVMProgramCopy& selfUpdate() {
			auto to_add = std::views::drop(
				original_program->getFunctions().allData(), static_cast<ssize_t>(functions.size())
			);

			for (auto& [low_func_data, oid, sid]: to_add)
				functions.insert(*low_func_data.get(), sid);

			return *this;
		}

		/**
		 * @brief Replaces opcode in provided function at provided index with provided opcode.
		 * @returns Original opcode from provided location on success and `nullopt` if location does
		 * not exist.
		 */
		base::Optional<MicroOpcode> replaceOpcode(
			usize function_id, usize instruction_index, MicroOpcode opcode
		) {
			if (!functions.contains(function_id)) return std::nullopt;

			auto& microbytecode = functions[function_id].bc;
			if (microbytecode.size() <= instruction_index) return std::nullopt;

			auto&       instruction     = microbytecode[instruction_index];
			MicroOpcode original_opcode = getInstructionOpcode(instruction);

			instruction = makeLowInstruction(opcode, instruction.arg0, instruction.arg1);

			return original_opcode;
		}
	};
}

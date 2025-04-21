/**
 * @file builtin_functions.hpp
 * @author Wojciech Rzepliński
 * @brief The implemention of the builtin functions in the VM.
 *
 * The builtin functions have custom C++ implementation that can interact with the outside world
 * but also with the VM's thread and process (like set thread status to "waitingForInput").
 *
 * Builtin functions have type that doesn't need to be declared before usage
 * because their declarations are always added to the program types 
 *(but they can be redefined as long as the types are the same).
 *
 * The goal of this implementation is to have one source file for the builtin functions -
 * this file. They have to be consistent with the HELIOS builtin list and LLVM bultins manually.
 */
#pragma once


#include <base/raw_view.hpp>
#include <base/string_id.hpp>
#include <base/stringifyable_enum.hpp>

#include <vm/bytecode/type_of_data.hpp>

namespace vm {
	class VMThread;

	namespace builtins {

		struct NoValue {};

		using Value = std::variant<i64, NoValue>;
		// I wanted to keep the parsing from byte array to value in the VMThread, because
		// how values are stored is it's responsibility.
		using FunctionHandler = std::function<Value(VMThread&, const std::vector<Value>&)>;

		struct Function {
			// If in the bytecode there is redefinition of this type
			// then the
			code::FunctionType type;  // Type can also be defined in "VM standard library" that
			                          // would be a connection between the
			// builtins and the VM.
			FunctionHandler handler;
		};

		/**
		 * @brief Class for FunctionHandlers.
		 *
		 * Each handler should be defined as static and have @p FunctionHandler type.
		 * The only reason this class exists is to enable the VMThread class to
		 * be a friend of the FunctionHandlers class, so the handlers can access
		 * the VMThread private members.
		 */
		class FunctionHandlers {
		public:
			static Value builtinInputI64Handler(VMThread& process, const std::vector<Value> &arguments);
			static Value builtinOutputI64Handler(VMThread& process, const std::vector<Value> &arguments);
		};

		/**
		 * @brief Returns the list of builtin functions with lazy initialization.
		 * @note Types here should match HELIOS types.
		 * The types used for parameters and return value are defined in the @file
		 * bytecode/builtin_types.hpp file.
		 */
		const std::array<Function, 2>& getBuiltinFunctions();

		/**
		 * @brief Get the ID of the builtin function by name.
		 * It's ID is the index in the BUILTIN_FUNCTIONS array.
		 *
		 * The ID is used in the LowVMProgram to store opcode
		 * arguments as numbers.
		 */
		base::Optional<usize> getBuiltinFunctionID(base::StrID name);

		/**
		 * @brief Fast lookup of the builtin function by ID.
		 *
		 * Used by the VMThread opcode implementation.
		 */
		inline CRef<Function> getBuiltinFunction(usize id) {
			if (id >= getBuiltinFunctions().size()) CORE_PANIC("Invalid builtin function ID: ", id);
			return &getBuiltinFunctions().at(id);
		}
	}
}

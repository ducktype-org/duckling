/**
 * @file builtin_functions.hpp
 * @author Wojciech Rzepliński
 * @brief The implemention of the builtin functions in the VM.
 *
 * The builtin functions have custom C++ implementation that can interact with the outside world
 * but also with the VM's thread and process (like set thread status to "waitingForInput").
 *
 * Each builtin function have it's own function type that doesn't need to be declared before usage
 * because their declarations are always added to the program types.
 * While performing validation checks on the program they behave like usual functions.
 *
 * The goal of this implementation is to have one source file for the builtin functions -
 * this file. They have to be consistent with the HELIOS builtin list and LLVM builtins manually.
 */
#pragma once


#include <base/raw_view.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>

namespace vm {
	class VMThread;

	namespace builtins {

		struct NoValue {};

		// Because VMThread stores values on the local stack as bytes,
		// there was a question of where the conversion from bytes to
		// value should be done. I decided that the conversion should be
		// in the VMThread. So the handlers only see the converted values.
		using Value           = std::variant<i64, NoValue>;
		using FunctionHandler = std::function<Value(VMThread&, const std::vector<Value>&)>;

		struct Function {
			/**
			 * Type declaration of the builtin that is always added to the program types.
			 */
			code::FunctionType type;

			/**
			 * You should call this function with arguments to call a builtin.
			 */
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
			static Value builtinInputI64Handler(
				VMThread& process, const std::vector<Value>& arguments
			);
			static Value builtinOutputI64Handler(
				VMThread& process, const std::vector<Value>& arguments
			);
		};

		/**
		 * @brief Returns the list of builtin functions with lazy initialization.
		 * @note Function types here should match HELIOS types.
		 * The types used for the parameters and the return value are defined in the @file
		 * bytecode/builtin_types.hpp file (like "i64", "i32", "void").
		 */
		const std::array<Function, 2>& getBuiltinFunctions();

		/**
		 * @brief Get the ID of the builtin function given the name.
		 * ID is the index in the BUILTIN_FUNCTIONS array.
		 *
		 * The ID is used in the LowVMProgram to store opcode
		 * arguments as numerical values (not strings).
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

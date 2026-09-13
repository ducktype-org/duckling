#pragma once

#include "instructions.hpp"

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>
#include <token_parser_core/common_elements.hpp>

#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief Represents an identifier (e.g. symbol name) as string with ElementBase
	 * (SourcePosition).
	 */
	struct Identifier final: ElementBase {
		Identifier() = default;

		Identifier(base::StrID str): str(str) {}

		Identifier(tpc::Identifier tpc_identifier):
			  ElementBase(tpc_identifier.position),
			  str(tpc_identifier.value) {}

		base::StrID str;

		bool operator==(const Identifier& other) const noexcept { return str == other.str; }

		operator base::StrID() const { return str; }
	};

	/**
	 * @brief Represents a block of instructions.
	 */
	using CodeBlock = std::vector<Instruction>;

	/**
	 * @brief Represents global data, like a constant or a variable.
	 */
	struct GlobalData final: ElementBase {
		Identifier name;
		Identifier type;

		base::Optional<Identifier> ctor_name;
		base::Optional<Identifier> dtor_name;

		// Currently unused.
		// @TODO: #2916 use this flag
		bool is_constant{ false };

		base::Optional<ConstantValue> initial_value;
	};

	struct FuncSignature final {
		std::vector<Identifier> result_types;
		std::vector<Identifier> parameters;

		bool operator==(const FuncSignature& other) const noexcept = default;
	};

	/**
	 * @brief Represents bytecode a function.
	 * @note A function on its own (without type context or globals) does not contain enough
	 * information to tell if it is correct/valid or not.
	 */
	struct Function final: ElementBase {
		Identifier    name;
		CodeBlock     body;
		FuncSignature signature;
	};

	/**
	 * @brief Represents a native function declared in bytecode and resolved by its name from one
	 * of the shared objects declared with `ffi object`. Called via `call_ffifunc` using libffi.
	 */
	struct FFIFunction final: ElementBase {
		Identifier    name;
		FuncSignature signature;

		/// Native symbol address, resolved during validation. Never null in a `ValidProgram`.
		void (*symbol)() = nullptr;
	};

	/**
	 * @brief Represents C/C++ function, that can be called from bytecode by its name.
	 * It's required that function accepts two parameters:
	 * - std::byte* destination - a place to store the call result
	 * - std::byte* arguments - arguments passed directly from the VM
	 * It's also required, that the VM types are trivially copyable.
	 */
	struct ExternalCFunction final {
		Identifier name;
		void (*function_pointer)(std::byte*, std::byte*) = nullptr;
		FuncSignature signature;
	};

	/**
	 * @brief Represents a group of types, globals, and functions.
	 * @note It's not guaranteed that every code collection is valid.
	 */
	struct CodeCollection final {
		std::vector<Function>          functions;
		std::vector<TypeOfData>        types;
		std::vector<GlobalData>        global_data;
		std::vector<ExternalCFunction> external_c_functions;
		std::vector<FFIFunction>       ffi_functions;
		/// Shared objects for FFI symbol resolution, each stored as the exact string passed to
		/// `dlopen` (an absolute path, or a bare name searched in the system library paths).
		std::vector<std::string> object_files;

		/**
		 * @brief Merges another CodeCollection into this one by appending all its elements.
		 * Does not perform any assertions.
		 */
		void mergeFrom(CodeCollection&& other);
	};
}

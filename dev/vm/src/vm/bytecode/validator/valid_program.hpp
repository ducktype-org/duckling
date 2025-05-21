#pragma once

#include "base/box.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

namespace vm::code {
	/**
	 * @brief State of the object represents a valid bytecode class.
	 * For a bytecode to be valid it means, that every function considered in the context stored in
	 * by this object, its instructions have correct arguments. Correct is a strong word here, as
	 * it means, that types match, there are no invalid jumps, stack is used correctly, etc.
	 *
	 * @note Valid in this context also does not mean it contains a `main` function, as this is not
	 * valid bytecode's assumption, but rather VMThread's.
	 */
	class ValidProgram {
		ValidProgram();

	public:
		ValidProgram(const ValidProgram&)            = default;
		ValidProgram(ValidProgram&&) noexcept        = default;
		ValidProgram& operator=(const ValidProgram&) = default;
		ValidProgram& operator=(ValidProgram&&)      = default;

		/**
		 * @brief Creates a new ValidProgram with nothing inside.
		 */
		static ValidProgram empty();

		/**
		 * @brief Creates a new ValidProgram object with builtin types pre-inserted.
		 */
		static ValidProgram withBuiltins();

		/**
		 * @brief Creates a CodeCollection that is validated.
		 */
		CodeCollection produceValidBytecode() const;

		/**
		 * @brief Produces TypeMetadata, that is isomorphic with its state.
		 * @TODO: Fix an issue, that TypeMetadata has to be built twice.
		 */
		Box<TypeMetadata> produceTypeMetadata() const;

		/**
		 * @brief Creates a new ValidProgram object with inserted code.
		 * @note If the newly injected code where to create an unvalid state,
		 * an exception of ValidationError base will be thrown.
		 */
		ValidProgram withCode(const std::vector<code::CodeCollection>& collections_to_add) const;

	private:
		/**
		 * @brief Inserts types.
		 * @TODO: Improve comment.
		 */
		void insertTypes(const std::vector<code::TypeOfData>& new_types);

		/**
		 * @brief Inserts globals.
		 * @TODO: Improve comment.
		 */
		void insertGlobals(const std::vector<code::GlobalData>& new_globals);

		/**
		 * @brief Inserts a function.
		 * @note It also performs removal of dead-code. We might modify this in the future, that
		 * function must not contain any dead-code, but Duckling's compiler, as of 21.05.2025, may
		 * produce dead code.
		 */
		void insertFunctions(const std::vector<code::Function>& new_functions);


		StableTypeIdNameMap<code::Function>   functions;
		StableTypeIdNameMap<code::GlobalData> globals_map;

		code::TypeContextValidator type_context;
	};
}

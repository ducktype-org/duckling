#pragma once

#include <base/box.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <unordered_set>

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
		ValidProgram() = default;

	public:
		bool is_stdlib_included = false;

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
		 * @brief Creates a new ValidProgram object with builtin types and builtin functions
		 * pre-inserted.
		 */
		static ValidProgram withStdlib();

		/**
		 * @brief Produces valid CodeCollection.
		 */
		CodeCollection produceValidCodeCollection() const;

		/**
		 * @brief Produces TypeMetadata, that is isomorphic with its state.
		 * @TODO: Fix an issue, that TypeMetadata has to be built twice.
		 */
		Box<TypeMetadata> produceTypeMetadata() const;

		/**
		 * @brief Creates a new ValidProgram object with inserted code.
		 * @note If the newly injected code were to create an unvalid state,
		 * an exception of ValidationError base will be thrown.
		 */
		ValidProgram newInsertCode(const code::CodeCollection& collection) const;

		/**
		 * @brief Inserts code in-place.
		 * @note If the newly injected code invalidates the state,
		 * an exception of `ValidationError` base is thrown. This means this object will contain
		 * invalid code and mustn't be used! If you don't want to lose the state, place use
		 * `newInsertCode`.
		 */
		void insertCode(const code::CodeCollection& collection);

		const ObjIdNameMap<TypeOfData>& types() const;

		const ObjIdNameMap<GlobalData>& globals() const;

		const ObjIdNameMap<Function>& functions() const;

	private:
		/**
		 * @brief Inserts types. May invalidate state.
		 * Can insert the same type multiple times.
		 */
		void insertTypes(const std::vector<code::TypeOfData>& new_types);

		/**
		 * @brief Inserts globals. May invalidate state.
		 * Cannot insert the same global data multiple times.
		 */
		void insertGlobals(const std::vector<code::GlobalData>& new_globals);

		/**
		 * @brief Inserts a function. May invalidate state.
		 * @note It also performs removal of dead-code. We might modify this in the future, that
		 * function must not contain any dead-code, but Duckling's compiler, as of 21.05.2025, may
		 * produce dead code.
		 */
		void insertFunctions(const std::vector<code::Function>& new_functions);

		ObjIdNameMap<code::Function>   function_map;
		ObjIdNameMap<code::GlobalData> globals_map;

		// Useful when verifying the presence of constructors and destructors while inserting globals.
		std::unordered_set<base::StrID> available_functions;

		code::TypeContext type_context;

		bool valid = true;
	};
}

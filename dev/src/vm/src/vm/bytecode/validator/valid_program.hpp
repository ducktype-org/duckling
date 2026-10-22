// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "flag_context.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_function.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/core/native/dynamic_library.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <memory>

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
	public:
		ValidProgram()                               = default;
		ValidProgram(const ValidProgram&)            = default;
		ValidProgram(ValidProgram&&) noexcept        = default;
		ValidProgram& operator=(const ValidProgram&) = default;
		ValidProgram& operator=(ValidProgram&&)      = default;

		/**
		 * @brief Creates a new ValidProgram object with builtin types pre-inserted.
		 */
		static ValidProgram withBuiltins();

		/**
		 * @brief Produces valid CodeCollection from the internal state of this object.
		 */
		CodeCollection produceValidCodeCollection() const;

		/**
		 * @brief Creates a new ValidProgram object with inserted code.
		 * @note If the newly injected code were to create an unvalid state,
		 * an exception of ValidationError base will be thrown.
		 */
		ValidProgram tryInsertCode(const CodeCollection& collection, api::ExecutionConfig config)
			const;

		const valid_type::ValidTypeMap& types() const;

		const TypeContext& getTypeContext() const;

		const ObjIdNameMap<GlobalData>& globals() const;

		const ObjIdNameMap<valid_function::ValidFunction>& functions() const;

		const ObjIdNameMap<ExternalCFunction>& extCFunctions() const;

		const ObjIdNameMap<FFIFunction>& ffiFunctions() const;

		const std::vector<std::pair<std::string, std::shared_ptr<native::DynamicLibrary>>>& objectFiles(
		) const;

	private:
		ObjIdNameMap<valid_function::ValidFunction> function_map;
		ObjIdNameMap<ExternalCFunction>             ext_c_function_map;
		ObjIdNameMap<FFIFunction>                   ffi_function_map;
		ObjIdNameMap<GlobalData>                    globals_map;
		TypeContext                                 type_context;
		FlagContext                                 flag_context;

		/**
		 * @brief Shared objects declared with `ffi object`, loaded into the process and keyed by
		 * the exact string passed to `dlopen`. Kept in insertion order so symbol resolution is
		 * deterministic (the earliest loaded object wins, like a linker). The library handles are
		 * shared between program copies and live for the lifetime of the program.
		 */
		std::vector<std::pair<std::string, std::shared_ptr<native::DynamicLibrary>>> object_files;

		/**
		 * @brief Contains a mapping from function name to function signature for all functions
		 * available in the program (not including builtin and external C functions). Used for type
		 * verification of class and interface types to check if implementations of declared methods
		 * match the expected signatures. This map basically stores forward declarations of functions
		 * available in the program, since `function_map` building is done after type verification.
		 */
		base::HashMap<base::StrID, FuncSignature> function_signatures;

		/**
		 * @brief Inserts code in-place.
		 * @note If the newly injected code invalidates the state, an exception of `ValidationError`
		 * base is thrown. This means this object will contain invalid code and mustn't be used! If
		 * you don't want to lose the state, place use `tryInsertCode`.
		 */
		void insertCode(const CodeCollection& collection, api::ExecutionConfig config);

		/**
		 * @brief Inserts types. May invalidate state.
		 * Can insert the same type multiple times.
		 */
		void insertTypes(const std::vector<TypeOfData>& new_types);

		/**
		 * @brief Inserts globals. May invalidate state.
		 * Cannot insert the same global data multiple times.
		 */
		void insertGlobals(const std::vector<GlobalData>& new_globals);

		/**
		 * @brief Inserts a function. May invalidate state.
		 * @note It also performs removal of dead-code. We might modify this in the future, that
		 * function must not contain any dead-code, but Duckling's compiler, as of 21.05.2025, may
		 * produce dead code.
		 */
		void insertFunctions(const std::vector<Function>& new_functions, api::ExecutionConfig config);

		/**
		 * @brief Inserts an ExternalCFunction. May invalidate state.
		 * Cannot insert multiple ExternalCFunctions with the same name.
		 */
		void insertExternalCFunctions(const std::vector<ExternalCFunction>& new_functions);

		/**
		 * @brief Inserts an FFIFunction. May invalidate state.
		 * Cannot insert multiple FFIFunctions with the same name. Signature types must be
		 * FFI-compliant (see `ValidType::isFFICompliant`); fixed-size tables are additionally
		 * rejected at the top level, as C has no by-value arrays. Resolves each function's native
		 * symbol from the loaded object files.
		 */
		void insertFFIFunctions(const std::vector<FFIFunction>& new_functions);

		/**
		 * @brief Loads new shared objects into the process. May invalidate state.
		 * Object files already loaded (by the same dlopen string) are skipped.
		 */
		void insertObjectFiles(const std::vector<std::string>& new_files);
	};
}

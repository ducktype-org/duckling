// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "dvm_internal_fwd.hpp"

#include <debug_info/debug_info.hpp>
#include <lir/lir_structure/lir_structure_fd.hpp>

#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief A stateful collection of code lowered into VM bytecode.
	 * @note If used improperly, query_ctx might become a dangling reference.
	 */
	class DVMCodeBuilder final {
	public:
		/**
		 * @brief Construct a new DVMCodeBuilder object
		 *
		 * @param query_ctx
		 * @param build_debug_info Whether to build debug info for the module.
		 * @param is_comp_time_lowering Whether we are lowering the code to be loaded by the VM for
		 * compile time evaluation, or for the final output module.
		 */
		DVMCodeBuilder(
			query::Context& query_ctx,
			base::StrID     module_id,
			bool            build_debug_info,
			bool            is_comp_time_lowering
		);

		/**
		 * @brief Inserts a LIR unit into the module.
		 * This acts as a main entry point and a source of truth for lowering LIR to DVM,
		 * and should be preferred over using the other insert functions.
		 */
		void insertLIRUnit(const lir::LIRUnit& lir_unit);

		/**
		 * @brief Inserts a LIR function into the module.
		 */
		void insertLIRFunction(CRef<lir::Function> lir_function);

		/**
		 * @brief Inserts an extern C function into the module.
		 */
		void insertExternCFunction(const vm::code::ExternalCFunction& extern_func);

		/**
		 * @brief Inserts a LIR global into the module.
		 */
		void insertLIRGlobal(const lir::LIRGlobalData& lir_global);

		/**
		 * @brief Insert raw bytecode into a module.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

		/**
		 * @brief Produces the per-module bytecode collection without cross-module validation.
		 * Full validation is performed at link time after all modules are merged.
		 */
		[[nodiscard]] vm::code::CodeCollection build() const;

		/**
		 * @brief Builds the debug info for the module if the class
		 * was constructed with build_debug_info=true. Returns empty optional otherwise.
		 * @note Only the first call returns the value and the subsequent calls will
		 * return an empty optional.
		 */
		[[nodiscard]] base::Optional<debug_info::DebugInfo> buildDebugInfo();

	private:
		/**
		 * We friend ReplDVMCodeBuilder, so it can access program_context directly.
		 */
		friend class ReplDVMCodeBuilder;

		/**
		 * A Boxed pointer to allow forward declaration in order to hide implementation details.
		 */
		Box<internal::ProgramLoweringContext> program_context;

		bool build_debug_info;
	};
}

#pragma once

#include <backends/dvm/dvm_backend.hpp>
#include <backends/dvm/dvm_internal_fwd.hpp>
#include <lir/lir_structure/lir_structure_fd.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief A stateful collection of code lowered into VM bytecode dedicated for REPL/scripts
	 * compilation use. It exposes an interface for incremental DVM code emission, allowing REPL
	 * statements to be compiled and loaded one at a time.
	 *
	 * @note Underneath it uses ProgramLoweringContext snapshot API that was added specifically for
	 * this use case, and DVMCodeBuilder lowering API.
	 *
	 * @important: It relies, to an extent, on the private implementation details of DVMCodeBuilder.
	 *
	 * @note If used improperly, query_ctx might become a dangling reference.
	 *
	 * The wrapper maintains a persistent DVMCodeBuilder state across REPL statements.
	 * It enables incremental bytecode loading without recompiling entire modules from scratch.
	 *
	 * @TODO: #2872 Move repl specific logic from ProgramLoweringContext into this wrapper, so that
	 * ProgramLoweringContext can be used for other purposes without carrying unnecessary
	 * REPL-specific state, logic and API. Maybe think a little bit more generally about
	 * implementation of this wrapper and its relation to ProgramLoweringContext and DVMCodeBuilder.
	 * One idea is to just remove ReplDVMCodeBuilder and add snapshotting api to DVMCodeBuilder.
	 */
	class ReplDVMCodeBuilder final {
	public:
		explicit ReplDVMCodeBuilder(query::Context& query_ctx, bool is_comp_time_lowering);
		~ReplDVMCodeBuilder();

		// Non-copyable, movable
		ReplDVMCodeBuilder(const ReplDVMCodeBuilder&)            = delete;
		ReplDVMCodeBuilder& operator=(const ReplDVMCodeBuilder&) = delete;
		ReplDVMCodeBuilder(ReplDVMCodeBuilder&&) noexcept;
		ReplDVMCodeBuilder& operator=(ReplDVMCodeBuilder&&) noexcept;


		/**
		 * @brief Set the query context for error reporting during compilation.
		 *
		 * Should be called when entering a query scope with active context.
		 * Must be paired with invalidateContext() when exiting the scope.
		 */
		void setContext(query::Context& query_ctx);

		/**
		 * @brief Clear the query context after compilation.
		 *
		 * Should be called when exiting the query scope to prevent dangling references.
		 */
		void invalidateContext();

		/**
		 * @brief Get the currently set query context.
		 *
		 * @return Optional reference to the active query context.
		 */
		[[nodiscard]] base::Optional<Ref<query::Context>> getActiveContext() const;

		/**
		 * Lowers a LIR unit into DVM bytecode and collects the newly lowered code.
		 *
		 * @note The newly lowered code might not include all entities from the LIR unit,
		 * as some of them might have been lowered in previous statements and are already present in
		 * the context.
		 */
		vm::code::CodeCollection insertLIRUnitAndCollectNewlyLoweredCode(
			const lir::LIRUnit& lir_unit
		);

		/**
		 * @brief Insert raw bytecode into a module.
		 * @note We can extend this function to return the CodeCollection of the new symbols only
		 * if we want to.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

	private:
		/**
		 * The DVMCodeBuilder instance used for lowering LIR units into DVM bytecode.
		 * @note DVMCodeBuilder friends ReplDVMCodeBuilder, so it can access program_context
		 * directly, which is necessary for the snapshotting logic.
		 */
		DVMCodeBuilder code_builder;
	};
}

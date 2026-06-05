#pragma once

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
	 * @brief A statefull collection of code lowered into VM bytecode dedicated for REPL/scripts compilation use.
	 * It exposes an interface for incremental DVM code emission, allowing REPL statements to be compiled and loaded one at a time.
	 * 
	 * @note underneath it uses ProgramLoweringContext snapshot api that was added specifically for this use case.
	 *
	 * @note If used improperly, query_ctx might become a dangling reference.
	 *
	 * This class provides a stable public interface to the DVM backend's internal lowering context,
	 * preventing REPL and other clients from directly depending on `src_private` implementation
	 * details. It delegates the actual lowering logic to `ProgramLoweringContext` while
	 * encapsulating incremental code emission for REPL use cases.
	 *
	 * The wrapper maintains a persistent lowering context across REPL statements, allowing
	 * later statements to reference symbols (functions, globals, types) from earlier ones.
	 * It exposes snapshot/collection semantics to enable incremental bytecode loading without
	 * recompiling the entire module.
	 * 
	 * PR: ADD a TODO here about moving logic up from ProgramLoweringContext 
	 */
	class ReplDVMCodeBuilder final {
	public:
		explicit ReplDVMCodeBuilder(query::Context& query_ctx);
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
		 * as some of them might have been lowered in previous statements and are already present in the context.
		 */
		vm::code::CodeCollection insertLIRUnitAndCollectNewlyLoweredCode(const lir::LIRUnit& lir_unit);

	private:

		// Pimpl: store pointer to complete type, with details in CPP
		base::Box<internal::ProgramLoweringContext> m_context;
	};

	/**
	 * @brief Create a new persistent program lowering context for REPL.
	 *
	 * The returned context maintains state across multiple REPL statement compilations,
	 * allowing later statements to reference symbols (functions, globals, types) defined
	 * in earlier statements without recompiling everything into a single module.
	 *
	 * @param query_ctx The query context used for error reporting.
	 * @return A box-managed ProgramLoweringContext. The context is owned by the caller
	 *         and must be kept alive for the duration of the REPL session.
	 */
	base::Box<internal::ProgramLoweringContext> createReplLoweringContext(query::Context& query_ctx);
}

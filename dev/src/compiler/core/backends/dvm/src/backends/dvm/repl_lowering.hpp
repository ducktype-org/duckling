#pragma once

#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext;
}

namespace compiler::backend_vm {
	/**
	 * @brief Wrapper for REPL-specific program lowering context.
	 *
	 * This class is a convenience wrapper that owns and manages a ProgramLoweringContext
	 * for use in REPL sessions. The underlying context maintains state across multiple
	 * REPL statement compilations, allowing later statements to reference symbols
	 * (functions, globals, types) defined in earlier statements.
	 *
	 * @note This wrapper doesn't add any complicated logic,
	 * it is a simple wrapper for the ProgramLoweringContext.
	 *
	 * @note Feel Free to refactor this class if a better way of managing
	 * the REPL context is found.
	 */
	class ReplLoweringContext final {
	public:
		explicit ReplLoweringContext(query::Context& query_ctx);
		~ReplLoweringContext();

		// Non-copyable, movable
		ReplLoweringContext(const ReplLoweringContext&)            = delete;
		ReplLoweringContext& operator=(const ReplLoweringContext&) = delete;
		ReplLoweringContext(ReplLoweringContext&&) noexcept;
		ReplLoweringContext& operator=(ReplLoweringContext&&) noexcept;

		/**
		 * @brief Get the underlying persistent program lowering context.
		 *
		 * This context accumulates all lowered functions, globals, and types across
		 * all REPL modules in this session.
		 */
		internal::ProgramLoweringContext& getContext();

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
		 * @brief Lower a LIR function into DVM bytecode function.
		 * @note If the function was already lowered, this is a no-op.
		 */
		const vm::code::Function& lowerAndKeepLirFunction(base::CRef<lir::Function> lir_function);

		/**
		 * @brief Lower a LIR global with its constructor and destructor.
		 */
		const vm::code::GlobalData& lowerAndKeepLirGlobal(
			const lir::LIRGlobal&                     lir_global,
			base::Optional<base::CRef<lir::Function>> global_ctor,
			base::Optional<base::CRef<lir::Function>> global_dtor
		);

		/**
		 * @brief Lower a LIR type layout into VM bytecode type representation.
		 * It caches the result, so inserts the type into the program only if needed.
		 *
		 * @return The DVM type corresponding to the TypeLayout, or an empty optional for layouts
		 * with no DVM counterpart (e.g. void).
		 */
		base::Optional<CRef<vm::code::TypeOfData>> lowerAndKeepTslType(
			base::CRef<tsl::TypeLayout> layout
		);

		/**
		 * @brief Check if a query context is currently set.
		 *
		 * @return true if a query context has been set via setContext() and not yet
		 *         invalidated, false otherwise.
		 */
		[[nodiscard]] bool hasContext() const;

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

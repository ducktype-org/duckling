#pragma once

#include "dvm_value.hpp"

#include <debug_info/debug_info_builder.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext {
		/**
		 * @brief Context used purely for throwing NotYetImplemented errors.
		 * @note This context should not be used for anything other than throwing NotYetImplemented
		 * errors.
		 * Remove this field when applicable.
		 */
		base::Optional<Ref<query::Context>> query_ctx_for_errors;

	public:
		/**
		 * @brief Construct with an initial query context (backward compatible).
		 *
		 * This constructor is provided for backward compatibility with existing call sites.
		 * The context reference should remain valid for the lifetime of this object.
		 */
		explicit ProgramLoweringContext(
			query::Context& query_ctx, bool build_debug_info, bool is_comp_time_lowering
		);

		/**
		 * @brief Set the query context for error reporting during compilation.
		 *
		 * Should be called when entering a query scope with active context.
		 * Must be paired with invalidateContext() when exiting the scope.
		 */
		void setContext(query::Context& query_ctx) { query_ctx_for_errors = &query_ctx; }

		/**
		 * @brief Clear the query context after compilation.
		 *
		 * Should be called when exiting the query scope to prevent dangling references.
		 */
		void invalidateContext() { query_ctx_for_errors = std::nullopt; }

		/**
		 * @brief Get the currently set query context.
		 *
		 * @return Optional reference to the active query context.
		 */
		[[nodiscard]] base::Optional<Ref<query::Context>> getActiveContext() const {
			return query_ctx_for_errors;
		}

		/**
		 * @brief Lowers a LIR function into DVM bytecode function.
		 * @note If the function was already lowered, this is a no-op.
		 */
		const vm::code::Function& lowerAndKeepLirFunction(CRef<lir::Function> lir_function);

		const vm::code::GlobalData& lowerAndKeepLirGlobal(
			const lir::LIRGlobal&               lir_global,
			base::Optional<CRef<lir::Function>> global_ctor,
			base::Optional<CRef<lir::Function>> global_dtor
		);

		/**
		 * @brief Lowers a LIR type layout into VM bytecode type representation.
		 * It caches the result, so inserts the type into the program only if needed.
		 */
		const vm::code::TypeOfData& lowerAndKeepTslType(CRef<tsl::TypeLayout> layout);

		/**
		 * @brief Creates and inserts a pointer type into the program lowering context.
		 * It caches the result, so inserts the type into the program only if needed.
		 */
		const vm::code::TypeOfData& getOrInsertPointerType(const vm::code::TypeOfData& pointee_type);

		/**
		 * @brief Retrieves the DVM global variable corresponding to the given LIR global.
		 * @note The LIR global must have been previously declared using insertLirGlobal,
		 * panics otherwise.
		 */
		[[nodiscard]] const DVMPlace& getLirGlobal(CRef<lir::LIRGlobal> lir_global) const;

		/**
		 * @brief Retrieves the extern C function with the given name.
		 * @note The extern C function must have been previously declared using
		 * insertExternCFunction, panics otherwise.
		 */
		[[nodiscard]] const vm::code::ExternalCFunction& getExternCFunction(
			const base::StrID& func_name
		) const;

		/**
		 * @brief Inserts an extern C function into the program context.
		 */
		void insertExternCFunction(const vm::code::ExternalCFunction& extern_func);

		/**
		 * @brief Insert raw bytecode into program context.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

		/**
		 * @brief Returns how many extra bytecode functions have been accumulated so far.
		 */
		[[nodiscard]] usize getExtraBytecodeFunctionCount() const {
			return extra_bytecode_functions.size();
		}

		/**
		 * @brief Returns how many lowered types have been accumulated so far.
		 */
		[[nodiscard]] usize getLoweredTypeCount() const { return lowered_type_order.size(); }

		/**
		 * @brief Returns how many lowered functions have been accumulated so far.
		 */
		[[nodiscard]] usize getLoweredFunctionCount() const {
			return lowered_function_order.size();
		}

		/**
		 * @brief Returns the lowered types added since @p start_index.
		 */
		[[nodiscard]] std::vector<vm::code::TypeOfData> getLoweredTypesSince(usize start_index
		) const;

		/**
		 * @brief Returns the lowered functions added since @p start_index.
		 */
		[[nodiscard]] std::vector<vm::code::Function> getLoweredFunctionsSince(usize start_index
		) const;

		/**
		 * @brief Returns the extra bytecode functions added since @p start_index.
		 */
		[[nodiscard]] std::vector<vm::code::Function> getExtraBytecodeFunctionsSince(usize start_index
		) const;

		/**
		 * @brief Produces the per-module bytecode collection.
		 *
		 * Assembles all lowered functions, globals, types, and extern C functions into a
		 * CodeCollection. No validation is performed here.
		 *
		 * @note It does not consume internal state and can be called multiple times.
		 */
		vm::code::CodeCollection produceCodeCollection();

		/**
		 * @brief Builds the debug info for the module
		 * if the class was constructed with debug info building enabled,
		 * returns nullopt otherwise.
		 * @note It leaves the internal debug info builder in an empty state,
		 * so subsequent calls to this method will return nullopt.
		 */
		[[nodiscard]] base::Optional<debug_info::DebugInfo> buildDebugInfo();

		[[nodiscard]] bool isCompTimeLowering() const;

	private:
		vm::code::TypeOfData lowerTslTypeInternal(CRef<tsl::TypeLayout> layout);

		/// Whether we are lowering the code to be loaded by the VM for compile time evaluation,
		/// or for the final output module. This affects how certain compile time values (e.g.
		/// symbol types) are lowered.
		bool is_comp_time_lowering;

		// Using ValidProgram here would be inefficient due to the need for frequent code verifications.

		struct TypeStorage {
			// Mapping from TSL layouts to names of DVM types which exist in `dvm_types`.
			base::Map<CRef<tsl::TypeLayout>, base::StrID> tsl_type_to_dvm_type_name;
			// Main container for all types in the module.
			base::HashMap<base::StrID, vm::code::TypeOfData> dvm_types;
		};

		// A set of types allowing for insertion of both TSL types and manual insertion of types.
		TypeStorage type_storage;
		// Maintains insertion order for types so REPL can emit only new types.
		std::vector<base::StrID> lowered_type_order;
		// Maintains insertion order for functions so REPL can emit only new functions.
		std::vector<vm::code::Function> lowered_function_order;

		base::Map<CRef<lir::Function>, vm::code::Function> lir_function_to_dvm;

		// Extern function name to definition.
		base::Map<base::StrID, vm::code::ExternalCFunction> extern_c_functions;

		// Additional, non-lir functions loaded into a module. Used in CTE.
		std::vector<vm::code::Function> extra_bytecode_functions;

		// Using names as keys to avoid issues with CRef hash/equality.
		base::HashMap<base::StrID, DVMPlace>             global_name_to_dvm;
		base::HashMap<base::StrID, vm::code::GlobalData> global_name_to_dvm_data;

		// Optional debug info builder.
		base::Optional<debug_info::DebugInfoBuilder> debug_info_builder;
	};
}

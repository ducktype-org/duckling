#pragma once

#include "dvm_value.hpp"

#include <backends/dvm/repl_lowering_snapshot.hpp>
#include <debug_info/debug_info_builder.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

namespace compiler::backend_vm::internal {
	class CTVLowering;

	class ProgramLoweringContext final {
		friend class CTVLowering;

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
			query::Context& query_ctx,
			base::StrID     module_id,
			bool            build_debug_info,
			bool            is_comp_time_lowering
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

		const vm::code::GlobalData& lowerAndKeepLirGlobal(const lir::LIRGlobalData& lir_global);

		/**
		 * @brief Lowers a LIR type layout into VM bytecode type representation.
		 * It caches the result, so inserts the type into the program only if needed.
		 * @return The DVM type corresponding to the TypeLayout or an empty optional for
		 * `tsl::EmptyTypeLayout`, representing Unit / empty-layout return types that do not
		 * have a DVM counterpart.
		 */
		base::Optional<CRef<vm::code::TypeOfData>> lowerAndKeepTslType(CRef<tsl::TypeLayout> layout);

		/**
		 * @brief Inserts a manually created DVM type into the program context and returns a
		 * reference to it. Used when want to register a type and use it by its name.
		 * @return CRef<vm::code::TypeOfData>
		 */
		CRef<vm::code::TypeOfData> keepVMType(vm::code::TypeOfData dvm_type);

		/**
		 * @brief Creates and inserts a pointer type into the program lowering context.
		 * It caches the result, so inserts the type into the program only if needed.
		 */
		const vm::code::TypeOfData& getOrInsertPointerType(const vm::code::TypeOfData& pointee_type);

		/**
		 * @brief Creates and inserts a pointer type based on the type name.
		 * It caches the result, so inserts the type into the program only if needed.
		 */
		const vm::code::TypeOfData& getOrInsertPointerType(base::StrID pointee_type_name);

		/**
		 * @brief Retrieves or lazily creates the DVM place for the given LIR global.
		 *
		 * This lookup is not purely observational: it may insert and cache a placeholder entry
		 * for the global. It is needed to reference globals from different modules.
		 *
		 * @note Returning a DVM place here does not necessarily mean that the corresponding
		 * vm::code::GlobalData has already been lowered for that global name.
		 */
		const DVMPlace& getLirGlobal(CRef<lir::LIRGlobal> lir_global);

		/**
		 * @brief Inserts a synthetic, statically-initialized global into the module and returns its
		 * DVM place.
		 *
		 * The global is marked constant and carries @p init as its `initial_value`. The given
		 * @p name_hint is made unique with an internal counter, so callers may reuse the same hint.
		 *
		 * @param name_hint Base name for the global (uniquified internally).
		 * @param type The DVM type of the global.
		 * @param init The static initial value bytes.
		 * @return The DVM place referring to the inserted global.
		 */
		const DVMPlace& insertStaticDataGlobal(
			base::StrID name_hint, const vm::code::TypeOfData& type, vm::code::ConstantValue init
		);

		/**
		 * @brief Produces a fresh, unique global name from @p name_hint.
		 *
		 * The hint is suffixed with the module id and an internal counter so the same hint can be
		 * reused for many anonymous globals (string literals, CTV values, ...).
		 */
		[[nodiscard]] base::StrID getAnonymousGlobalName(base::StrID name_hint);

		/**
		 * @brief Registers the DVM place of a global so it can be referenced.
		 *
		 * Inserts a `Direct`-access place for @p name / @p type into the place table and returns
		 * it. This only declares where the global lives; use @ref defineGlobal to attach its data.
		 */
		const DVMPlace& declareGlobal(base::StrID name, const vm::code::TypeOfData& type);

		/**
		 * @brief Stores the data of a global and records it in emission order.
		 *
		 * Keyed by @p global_data.name. Order is recorded only on first insertion so REPL can emit
		 * only new globals.
		 */
		const vm::code::GlobalData& defineGlobal(vm::code::GlobalData global_data);

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
		 * @brief Capture current counts of lowered entities.
		 */
		[[nodiscard]] compiler::backend_vm::LoweredEntitiesSnapshot captureLoweredEntitiesSnapshot(
		) const;

		/**
		 * @brief Collect newly lowered types/functions/extra functions since a snapshot.
		 */
		[[nodiscard]] vm::code::CodeCollection collectNewCodeSince(
			const compiler::backend_vm::LoweredEntitiesSnapshot& snapshot
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
		base::Optional<vm::code::TypeOfData> lowerTslTypeInternal(CRef<tsl::TypeLayout> layout);

		/// Whether we are lowering the code to be loaded by the VM for compile time evaluation,
		/// or for the final output module. This affects how certain compile time values (e.g.
		/// symbol types) are lowered.
		bool is_comp_time_lowering;

		base::StrID module_id;

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
		std::vector<base::StrID> lowered_function_order;
		// Maintains insertion order for globals so REPL can emit only new globals.
		std::vector<base::StrID> lowered_global_order;

		// Counter used to make synthetic static-data global names (string literals) unique.
		usize static_data_global_counter{ 0 };

		base::Map<CRef<lir::Function>, base::StrID> lir_function_to_name;
		base::Map<base::StrID, vm::code::Function>  dvm_functions_by_name;

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

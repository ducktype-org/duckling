#pragma once

#include "function_forward.hpp"  // IWYU pragma: keep

#include <helios/ctv/ctv.hpp>
#include <helios/hout/hout_fd.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/types/ok_bad.hpp>

#include <query_framework/context_fd.hpp>

#include <memory>
#include <utility>

// Doc style is intentional, caused by inexplicable funkiness in how Doxygen interacts with macros.
MAKE_STRINGIFYABLE_ENUM(compiler::lir, u64, Operation,
	/** Placeholder for uninitialized value, should not be in LIR output. */
	Uninitialized,

	/** Simple byte by byte assignment. */
	Assign,

	/**
		@brief Placeholder.
		@todo Some decisions here to be made about operations like that.
	*//**
		Perhaps we want more generic code for LIR, so algorithms are simpler.
		There could be single operation for all Add, Sub, etc, and single one for all comparisons.

		Some operations are sign-sensitive and are prefixed with U or S, e.g. UDiv and SDiv.
	*/
	IntegerAdd,
	IntegerSub,
	IntegerNeg,
	IntegerMul,
	IntegerUDiv,
	IntegerSDiv,
	IntegerUMod,
	IntegerSMod,

	IntegerULt,
	IntegerUGt,
	IntegerULteq,
	IntegerUGteq,
	IntegerSLt,
	IntegerSGt,
	IntegerSLteq,
	IntegerSGteq,
	IntegerEq,
	IntegerNeq,

	BooleanAnd,
	BooleanOr,
	BooleanNot,

	Cast,

	Call,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch
)

namespace compiler::lir {
	struct LirLocal;
	struct Block;
	struct Function;

	/**
	 * @brief Reference to local variable in LIR.
	 */
	using LirLocalRef = CRef<LirLocal>;

	/**
	 * @brief Reference to block in LIR.
	 */
	using BlockRef = CRef<Block>;

	/**
	 * @brief Reference to a function in LIR.
	 */
	struct FunctionLiteral {
		base::StrID                                   mangled_name;
		helios::SymbolABI                             abi;
		std::shared_ptr<std::vector<tsl::TypeLayout>> parameter_layouts;
		std::shared_ptr<tsl::TypeLayout>              return_type_layout;

		static FunctionLiteral fromFunction(const Function&);
	};

	/**
	 * @brief Description of a LIR Local variable or function argument.
	 * @note This structure should only be stored directly in LIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct LirLocal final {
		/**
		 * @brief HELIOS id of the variable, if exists.
		 */
		base::Optional<helios::SymID> helios_id;

		// @TODO: #1520 Introduce interning, store layouts cheaper.
		tsl::TypeLayout layout;

		/**
		 * @brief Index of the parameter in the function, if this is a function parameter.
		 */
		base::Optional<u64> parameter_index;

	private:
		LirLocal(
			const base::Optional<helios::SymID> helios_id,
			tsl::TypeLayout                     layout,
			base::Optional<u64>                 parameter_index
		):
			  helios_id(helios_id),
			  layout(std::move(layout)),
			  parameter_index(parameter_index) {}

		explicit LirLocal(tsl::TypeLayout layout): helios_id({}), layout(std::move(layout)) {}

		friend Function;
		friend LirLocalRef;

	public:
		/**
		 * @note Do not use this function outside of LIR lowering.
		 */
		static LirLocal fromMIR(query::Context& ctx, mir::MirLocalRef mir_local);

		/**
		 * @brief Crates unique local with bool-type, and without
		 * helios_id.
		 * @note it's used to create lifetime-flags
		 * @param ctx
		 * @return LirLocal
		 */
		static LirLocal boolLocal(query::Context& ctx);
	};

	enum class LirGlobalType { Variable, Constant };

	/**
	 * @brief Global variable in LIR.
	 * layout is in shared_ptr, so the LirGlobal can be copied
	 */
	struct LirGlobal final {
		/**
		 * @brief HELIOS id of the variable.
		 */
		helios::SymID helios_id;

		// @TODO: #1520 Introduce interning for layouts, use it here instead of shared_ptr.
		std::shared_ptr<tsl::TypeLayout> layout;

		base::StrID mangled_name;

		LirGlobalType type;

		base::Optional<helios::CompileTimeValue> initial_value;

	private:
		LirGlobal(
			const helios::SymID                      helios_id,
			const tsl::TypeLayout&                   layout,
			const base::StrID&                       mangled_name,
			const LirGlobalType                      type          = LirGlobalType::Variable,
			base::Optional<helios::CompileTimeValue> initial_value = {}
		):
			  helios_id(helios_id),
			  layout(std::make_shared<tsl::TypeLayout>(layout)),
			  mangled_name(mangled_name),
			  type(type),
			  initial_value(initial_value) {}

		friend Function;

	public:
		/**
		 * @note Do not use this function outside of LIR lowering.
		 */
		static LirGlobal fromMIR(query::Context& ctx, mir::MirGlobal mir_global);

		/**
		 * @note Do not use this function outside of LIR lowering.
		 */
		static LirGlobal fromHOUT(query::Context& ctx, const helios::HOUTGlobalData& helios_id);
	};

	/**
	 * @brief Represents access into a variable (local or global), or its component.
	 *
	 * For example, for an access like `a.b.c`, where `a` is a local or global variable,
	 * and `b` and `c` are fields within that variable, this structure would contain
	 * the base variable (`a`) and the access chain (`[b, c]`).
	 *
	 * For access to the whole variable (e.g., just `a`), the access chain would be empty.
	 */
	struct LirPlace final {
		using BaseVariant = std::variant<LirLocalRef, LirGlobal>;
		/**
		 * @brief Base of the LIR place, either local or global variable.
		 */
		BaseVariant base;

		/**
		 * @brief Get the type layout of the base variable.
		 */
		tsl::TypeLayout getBaseLayout() {
			variant_match(base) {
				variant_case(LirLocalRef, local) { return local->layout; }
				variant_case(LirGlobal, global) { return *global.layout; }
			}
			CORE_UNREACHABLE();
		}

		template<class T>
		const T& getBase() const {
			return std::get<T>(base);
		}

		/**
		 * @brief The symbols of the fields accessed within the variable.
		 */
		std::vector<helios::SymID> access_chain;

		/**
		 * @brief The type layout of the final accessed field.
		 * @note This type layout may be different from the layout of the base variable,
		 * especially when the access chain is not empty.
		 * @TODO: #1520 Introduce interning, store layouts cheaper.
		 */
		tsl::TypeLayout layout;

		LirPlace(query::Context& ctx, BaseVariant base, std::vector<helios::SymID> access_chain);

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<LirLocalRef>(base);
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<LirGlobal>(base);
		}

		[[nodiscard]]
		bool hasAccess() const {
			return !access_chain.empty();
		}
	};

	/**
	 * @brief Any value in LIR representation
	 */
	struct LIRValue {
	private:
		using ValueType = std::variant<i64, bool, LirPlace, BlockRef, FunctionLiteral>;
		ValueType value;

	public:
		LIRValue(i64 value): value(value) {}

		LIRValue(bool value): value(value) {}

		LIRValue(LirPlace value): value(value) {}

		LIRValue(BlockRef value): value(value) {}

		LIRValue(FunctionLiteral value): value(value) {}

		bool operator==(const LIRValue& other) const = default;

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
		}

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<LirPlace>(value) && std::get<LirPlace>(value).isLocal();
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<LirPlace>(value) && std::get<LirPlace>(value).isGlobal();
		}

		/**
		 * @brief Returns reference value of given type
		 * stored in LIRValue.
		 * Throws if value is not of given type.
		 * @tparam T
		 * @return const T&
		 */
		template<class T>
		const T& get() const {
			return std::get<T>(value);
		}
	};

	/**
	 * @brief Some instructions are parametrized by extra parameters.
	 * For example, cast instruction needs to know
	 * from which type to which type it is casting.
	 */
	struct NoInstrParameters final {};

	struct CastParameters final {
		tsh::SymbolType<> source_type;
		tsh::SymbolType<> target_type;
		// @TODO: #1520 Introduce interning for layouts, use it here instead of shared_ptr.
		std::shared_ptr<tsl::TypeLayout> source_layout;
		std::shared_ptr<tsl::TypeLayout> target_layout;
	};

	using InstrParameters = std::variant<NoInstrParameters, CastParameters>;

	/**
	 * @brief Single instruction of LIR code.
	 */
	struct Instruction final {
		Operation                operation = Operation::Uninitialized;
		base::Optional<LirPlace> output;
		std::vector<LIRValue>    arguments;
		InstrParameters          extra_params{ NoInstrParameters{} };

		// @TODO: each Instruction should have source position reference

		Instruction()                       = default;
		Instruction(const Instruction&)     = default;
		Instruction(Instruction&&) noexcept = default;

		Instruction& operator=(Instruction&&) noexcept = default;

		Instruction(
			const Operation          operation,
			base::Optional<LirPlace> output,
			std::vector<LIRValue>    arguments,
			InstrParameters          extra_parameters = NoInstrParameters{}
		):
			  operation(operation),
			  output(std::move(output)),
			  arguments(std::move(arguments)),
			  extra_params(std::move(extra_parameters)) {}
	};

	/**
	 * @brief LIR block.
	 * @note This structure should only be stored directly in LIR Function, as part of the
	 * description of a function. Other uses should use BlockRef to reference the block.
	 */
	struct Block final {
		std::vector<Instruction> instructions;
		Instruction              terminator;
	};

	/**
	 * @brief Function in LIR.
	 */
	struct Function final {
		base::StrID       mangled_name;
		helios::SymbolABI abi;

		tsl::TypeLayout              return_type_layout;
		std::vector<tsl::TypeLayout> parameter_layouts;

		base::StableVector<Block>    blocks;
		base::StableVector<LirLocal> local_list;

		std::vector<BlockRef> block_order;

		/**
		 * @brief Checks if block order uniquely stores
		 * all blocks.
		 */
		[[nodiscard]]
		base::OkBad validateBlockOrder() const;

		/**
		 * Checks if parameter types and parameter local variables are consistent,
		 * And if parameters indexes are correct.
		 */
		[[nodiscard]]
		base::OkBad validateParameters() const;

		void debugPrint(query::Context&, std::ostream& output) const;

		/**
		 * @brief Returns a map from all blocks to unique ids.
		 * @note Those ids do not cary any meaning, they are made here to be consistent in
		 * different part of compiler (e.g. lir printing, llvm lowering).
		 * @return base::Map<BlockRef, u64>
		 */
		[[nodiscard]]
		base::Map<BlockRef, u64> getBlockIDs() const;

		/**
		 * @brief Returns a map from all locals to unique ids.
		 * @note Those ids do not cary any meaning, they are made here to be consistent in
		 * different part of compiler (e.g. lir printing, llvm lowering).
		 * @return base::Map<BlockRef, u64>
		 */
		[[nodiscard]]
		base::Map<LirLocalRef, u64> getLocalVariableIDs() const;
	};
}

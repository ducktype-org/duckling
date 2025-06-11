#pragma once

#include "function_forward.hpp"

#include <mir/mir_structure/mir_local_ref.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/ok_bad.hpp>
#include <base/stable_container.hpp>
#include <base/stringifyable_enum.hpp>

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
	IntegerMul,
	IntegerUDiv,
	IntegerSDiv,
	IntegerUMod,
	IntegerSMod,
	IntegerULt,
	IntegerSLt,
	IntegerNeg,
	BooleanAnd,
	BooleanOr,
	BooleanNot,
	Call,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch
)

namespace compiler::lir {
	struct LirLocal;
	struct Block;

	/**
	 * @brief Reference to local variable in LIR.
	 */
	using LocalRef = CRef<LirLocal>;

	/**
	 * @brief Reference to block in LIR.
	 */
	using BlockRef = CRef<Block>;

	/**
	 * @brief Reference to a function in LIR.
	 * @note In the future this might simple store mangled name (string), and possibly an optional
	 * SymID.
	 */
	struct FunctionLiteral {
		helios::SymID helios_id;
	};

	/**
	 * @brief Any value in LIR representation
	 */
	struct LIRValue {
	private:
		using ValueType = std::variant<i64, bool, LocalRef, BlockRef, FunctionLiteral>;
		ValueType value;

	public:
		LIRValue(i64 value): value(value) {}

		LIRValue(bool value): value(value) {}

		LIRValue(LocalRef value): value(value) {}

		LIRValue(BlockRef value): value(value) {}

		LIRValue(FunctionLiteral value): value(value) {}

		bool operator==(const LIRValue& other) const = default;

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
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

		// a copy of type-layout here might be suboptimal
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
		friend LocalRef;

	public:
		// note: don't use it outside lir lowering:

		static LirLocal fromMir(query::Context& ctx, mir::LocalRef mir_local);

		/**
		 * @brief Crates unique local with bool-type, and without
		 * helios_id.
		 * @note it's used to create lifetime-flags
		 * @param ctx
		 * @return LirLocal
		 */
		static LirLocal boolLocal(query::Context& ctx);
	};

	/**
	 * @brief Single instruction of LIR code.
	 */
	struct Instruction final {
		Operation                operation = Operation::Uninitialized;
		base::Optional<LocalRef> output;
		std::vector<LIRValue>    arguments;

		// @TODO: each Instruction should have source position reference

		Instruction()                   = default;
		Instruction(const Instruction&) = default;
		Instruction(Instruction&&)      = default;

		Instruction& operator=(Instruction&&) = default;

		Instruction(
			const Operation                operation,
			const base::Optional<LocalRef> output,
			std::vector<LIRValue>          arguments
		):
			  operation(operation),
			  output(output),
			  arguments(std::move(arguments)) {}
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
		base::StrID mangled_name;

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
		base::Map<LocalRef, u64> getLocalVariableIDs() const;
	};
}

#pragma once

#include <typesystem/lower/type_layout.hpp>
#include <base/stable_container.hpp>
#include <base/stringifyable_enum.hpp>

#include <mir/mir_structure/mir_local_ref.hpp>

// clang-format off

// @TODO:
// once this is introduced: https://github.com/orgs/ducktype-org/projects/11/views/1?pane=issue&itemId=86181552
// change name to just Operation
MAKE_STRINGIFYABLE_ENUM(compiler::lir, u64, LirOperation,
	Uninitialized, //< placeholder for uninitialized value, should not be in LIR output

	Assign, //< simple byte by byte assignment

	/**
	 * @brief Placeholder. 
	 * @todo  Some decisions here to be made about operations like that.
     * Perhaps we want more generic code for MIR, so algorithms are simpler.
	 * There could be single operation for all Add, Sub, etc, and single one for all
	 * comparisons.
	 */
	IntegerAdd,
	
	ReturnVoid,
	ReturnValue,
	Jump,
	Branch
);

// clang-format on


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
	 * @brief Any value in LIR representation
	 */
	struct LirLocation {
	private:
		using ValueType = std::variant<i64, LocalRef, BlockRef>;
		ValueType value;

	public:
		LirLocation(i64 value): value(value) {}

		LirLocation(LocalRef value): value(value) {}

		LirLocation(BlockRef value): value(value) {}

		bool operator==(const LirLocation& other) const = default;

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
		}

		/**
		 * @brief Returns reference value of given type
		 * stored in LirLocation.
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
		 * @brief HELIOS id of the variable, if exist.
		 */
		base::Optional<helios::SymID> helios_id;

		// a copy of type-layout here might bu sub-optimal
		tsl::TypeLayout layout;


	private:
		LirLocal(helios::SymID helios_id, tsl::TypeLayout layout):
			  helios_id(helios_id),
			  layout(std::move(layout)) {}

		LirLocal(tsl::TypeLayout layout): helios_id({}), layout(std::move(layout)) {}

		friend struct Function;
		friend LocalRef;

	public:
		// note: don't use it outside lir lowering:

		static LirLocal fromMir(query::Context& ctx, mir::LocalRef mir_local);

		/**
		 * @brief Crates unique local with bool-type, and without
		 * helios_id.
		 * @note its used to create lifetime-flags
		 * @param ctx
		 * @return LirLocal
		 */
		static LirLocal boolLocal(query::Context& ctx);
	};

	/**
	 * @brief Single instruction of LIR code.
	 */
	struct Instruction final {
		LirOperation             operation = LirOperation::Uninitialized;
		base::Optional<LocalRef> output;
		std::vector<LirLocation> arguments;

		// @TODO: each Instruction should have source position reference

		Instruction()                   = default;
		Instruction(const Instruction&) = default;
		Instruction(Instruction&&)      = default;

		Instruction& operator=(Instruction&&) = default;

		Instruction(
			LirOperation             operation,
			base::Optional<LocalRef> output,
			std::vector<LirLocation> arguments
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
		// @TODO: store type of the function

		// @TODO: is this name mangled somehow:?
		base::StrID name;

		base::StableVector<Block>    blocks;
		base::StableVector<LirLocal> local_list;

		std::vector<BlockRef> block_order;

		/**
		 * @brief Checks if block order uniquely stores
		 * all blocks.
		 *
		 * @return true
		 * @return false
		 */
		[[nodiscard]]
		bool validateBlockOrder() const;

		void debugPrint(query::Context&, std::ostream& output) const;
	};
}

#pragma once

#include <vector>
#include <variant>
#include <helios/scopes/scopes.hpp>
#include <typesystem/higher/type_desc.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_id.hpp>
#include <base/stringifyable_enum.hpp>

#include "mir_local_ref.hpp"

// clang-format off
// Doc style is intentional, caused by inexplicable funkiness in how Doxygen interacts with macros.
MAKE_STRINGIFYABLE_ENUM(compiler::mir, u64, Operation,
	Uninitialized,

	Call,
	VCall,

	/** Simple byte by byte assignment */
	Assign,

	/**
		@brief Placeholder.
		@todo  Some decisions here to be made about operations like that.
	*//**
		Perhaps we want more generic code for MIR, so algorithms are simpler.
		There could be single operation for all Add, Sub, etc, and single one for all comparisons.
	*/
	IntegerAdd,
	IntegerSub,
	IntegerMul,
	IntegerDiv,
	IntegerMod,
	IntegerPow,
	IntegerLt,
	IntegerNeg,

	/** See readme.md for more info about destruct. */
	Destruct,
	/** See readme.md for more info about DestructIf. */
	DestructIf,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch,

	/**
		@brief Operation that represents end of a function.
		This operation can have different meaning depending on the context.
		For example in a function that returns void, it is just a return.
		In a function that returns value, "it is" an compiler error, unless it's unreachable.
		@note  It is always implicitly added at the end of a function.
	*/
	FunctionEnd
)

// clang-format on

namespace compiler::mir {

	/**
	 * @brief Whether given operation is an operation that can (and has to be)
	 * the last operation in the block (i.e. be a terminator).
	 */
	bool isTerminating(Operation);

	/**
	 * @brief BlockID is a temporary solution that should be replaced by
	 * proper BlockReference.
	 * It is like that for now, to avoid confusion with BlockRef used in mir_lowering and
	 * transformation between that BlockRef to this "BlockRef".
	 */
	STRONG_TYPEDEF_INT(BlockID, u64);

	struct MirIntegerConst final {
		i64 value;

		bool operator==(const MirIntegerConst& other) const = default;
	};

	struct MirBoolConst final {
		bool value;

		bool operator==(const MirBoolConst& other) const = default;
	};

	STRONG_TYPEDEF_ID(LocalID);

	/**
	 * @brief Description of a MIR Local variable, like a function argument or simply local
	 * variable.
	 * @note This structure should only be stored directly in MIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct MirLocal final {
		LocalID id;

		// Locals without a helios_id are temporary.
		base::Optional<helios::SymID> helios_id;
		tsh::TypeDesc<> type;
		helios::ScopeID lifetime_scope;

	private:
		// @note: Constructing MirLocal from helios_id
		// might work poorly for template/generic instantiations.

		MirLocal(const helios::SymID helios_id, const tsh::TypeDesc<> type, const helios::ScopeID lifetime_scope):
			  id(LocalID::next()),
			  helios_id(helios_id),
			  type(type),
			  lifetime_scope(lifetime_scope) {}

		MirLocal(const tsh::TypeDesc<> type, const helios::ScopeID lifetime_scope):
              id(LocalID::next()),
              helios_id({}),
              type(type),
              lifetime_scope(lifetime_scope) {}

		friend struct Function;
		friend struct FunctionBuilder;
		friend LocalRef;

	public:
		void debugPrint(std::ostream& output, bool detailed = false) const;

		[[nodiscard]]
		base::StrID getName() const;

		bool operator==(const MirLocal& other) const { return id == other.id; }
	};

	/**
	 * @brief Structure representing any MIR value.
	 */
	struct MirLocation final {
	private:
		// @TODO: global, literal, func-literal, ...
		// "LocalAccess" a.b.c
		// "GlobalAccess" a.b.c
		using ValueType = std::variant<MirIntegerConst, MirBoolConst, LocalRef, BlockID>;

		ValueType value;

	public:
		MirLocation(MirIntegerConst value): value(value) {}

		MirLocation(MirBoolConst value): value(value) {}

		MirLocation(LocalRef value): value(value) {}

		MirLocation(BlockID value): value(value) {}

		bool operator==(const MirLocation& other) const = default;

		void debugPrint(std::ostream& output) const;

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
		}

		/**
		 * @brief Returns reference value of given type
		 * stored in MirLocation.
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
	 * @brief Structure representing meta informations about operation
	 * such as:
	 * * does operation construct some variable
	 * * does operation destruct some variable
	 * * does operation move some variable
	 */
	struct OperationFlag final {
		enum class Flag { Construct, Destruct, Move };
		Flag     flag;
		LocalRef local;

		bool operator==(const OperationFlag& other) const = default;

		void debugPrint(std::ostream& output) const;
	};

	/**
	 * @brief Single instruction of MIR code.
	 *
	 */
	struct Instruction final {
		Operation operation = Operation::Uninitialized;

		base::Optional<LocalRef> output;

		std::vector<MirLocation> arguments;

		// construct, destruct, move.
		std::vector<OperationFlag> flags;

		// @TODO: each Instruction should have source position reference

		/**
		 * @brief Helios Scope this instruction comes from.
		 * Used for lifetime analysis.
		 * @todo: we might or might now want to create "MIR scopes" in the future.
		 */
		helios::ScopeID scope;

		Instruction()                   = delete;
		Instruction(const Instruction&) = default;

		/**
		 * @todo this line produces -Wmaybe-uninitialized warning for some reason.
		 * fix it.
		 */
		Instruction(Instruction&&) = default;

		Instruction(
			Operation                  operation,
			base::Optional<LocalRef>   output,
			std::vector<MirLocation>   arguments,
			std::vector<OperationFlag> flags,
			helios::ScopeID            scope
		):
			  operation(operation),
			  output(output),
			  arguments(std::move(arguments)),
			  flags(std::move(flags)),
			  scope(scope) {}

		bool operator==(const Instruction& other) const = default;

		void debugPrint(std::ostream& output) const;
	};

	/**
	 * @brief Returns list of MIR BlockIDs that
	 * can be jumped to from given terminator instruction.
	 *
	 * @param terminator
	 * @return std::vector<BlockID>
	 */
	std::vector<BlockID> getTerminatorSuccessors(const Instruction& terminator);

	/**
	 * @brief A simple block of MIR cfg code.
	 */
	struct Block final {
		/**
		 * @brief Unique identifier of the block.
		 * @note it must be identical to the index in the vector of blocks.
		 */
		BlockID id;

		/**
		 * @brief List of instructions in the block.
		 * @note It does not include terminator instruction.
		 */
		std::vector<Instruction> instructions;

		/**
		 * @brief Last instruction of the block.
		 * It has to be terminating instruction (branch, return, etc).
		 *
		 * @todo: Decide if we wan't to move it to instruction vector.
		 */
		Instruction terminator;

		bool operator==(const Block& other) const = default;

		[[nodiscard]]
		helios::ScopeID beginScope() const;
	};

	/**
	 * @brief Function in MIR.
	 */
	struct Function final {
		base::StrID                  name;
		std::vector<Block>           blocks;
		base::StableVector<MirLocal> local_list;
		BlockID                      entry_block;
		helios::ScopeID              top_lifetime_scope;

		// helios ID for hashes, ... this it temporary?
		// pushing this ID all the way here is problematic
		// it should be optional at best
		helios::SymID helios_id;

		Function()                = delete;
		Function(const Function&) = delete;
		Function(Function&&)      = default;

		Function& operator=(const Function&) = delete;

		// We can change it to default, when there will be a reason:
		Function& operator=(Function&&) = delete;

		Function(
			base::StrID                  name,
			std::vector<Block>           blocks,
			base::StableVector<MirLocal> local_list,
			BlockID                      entry_block,
			helios::ScopeID              top_lifetime_scope,
			helios::SymID                helios_id
		);

		[[nodiscard]]
		base::HashT customPerfectHash() const;

		bool operator==(const Function& other) const = default;

		void debugPrint(std::ostream& output) const;
	};

}

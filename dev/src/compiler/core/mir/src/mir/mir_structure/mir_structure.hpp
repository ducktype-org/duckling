#pragma once

#include "mir_lifetime_scope.hpp"
#include "mir_local_ref.hpp"
#include "mir_metadata.hpp"

#include <ctv/ctv.hpp>
#include <helios/tsh/types.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>

#include <utility>
#include <variant>
#include <vector>

// Doc style is intentional, caused by inexplicable funkiness in how Doxygen interacts with macros.
MAKE_STRINGIFYABLE_ENUM(compiler::mir, u64, Operation,
	Uninitialized,

	/** Empty instruction, sometimes used by the lowering to fill instruction holes that were not needed */
	Nop,

	Call,
	VCall,

	/** Simple byte by byte assignment */
	Assign,
	AddressOf,

	/**
		@brief Placeholder.
		@todo  Some decisions here to be made about operations like that.
	*//**
		Perhaps we want more generic code for MIR, so algorithms are simpler.
		There could be single operation for all Add, Sub, etc, and single one for all comparisons.
	*/
	IntegerAdd,
	IntegerSub,
	IntegerNeg,
	IntegerMul,
	IntegerDiv,
	IntegerMod,

	IntegerLt,    // Less than
	IntegerGt,    // Greater than
	IntegerLteq,  // Less than or equal to
	IntegerGteq,  // Greater than or equal to
	IntegerEq,    // Equal to
	IntegerNeq,   // Not equal to

	FloatAdd,
	FloatSub,
	FloatMul,
	FloatDiv,
	FloatNeg,

	FloatLt,    // Less than
	FloatGt,    // Greater than
	FloatLteq,  // Less than or equal to
	FloatGteq,  // Greater than or equal to
	FloatEq,    // Equal to
	FloatNeq,   // Not equal to

	BooleanAnd,
	BooleanOr,
	BooleanNot,


	/**
	 * @brief Operation on meta types for compile time function evaluation.
	 * The specific meta operation is parametrized by `MetaParameters` (a `MetaKind`) stored in the
	 * instruction's `extra_params`.
	 */
	MetaTypeOperation,

	/** Cast is also parametrized by the source type and the target type */
	Cast,

	/**
		Creates a variant value from a payload value.
		1 argument (the payload), parametrized by VariantParameters (the chosen alternative).
	*/
	VariantConstruct,
	/**
		Produces a pointer to the variant's payload if its active alternative matches the
		one in VariantParameters, a null pointer otherwise.
		1 argument: a reference to the variant, not the variant place itself.
	*/
	VariantTryProject,

	ZeroInitialize,

	/** See readme.md for more info about destruct. */
	Destruct,
	/** See readme.md for more info about DestructIf. */
	DestructIf,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch,
	/**
		Terminator: jumps to the first block argument if the pointer argument is null,
		to the second one otherwise. Arguments: [pointer, null_target, not_null_target].
	*/
	BranchIfNull,

	/**
		@brief Operation that represents end of a function.
		This operation can have different meaning depending on the context.
		For example in a function that returns void, it is just a return.
		In a function that returns value, "it is" an compiler error, unless it's unreachable.
		@note  It is always implicitly added at the end of a function.
	*/
	FunctionEnd
)

/**
    @brief The specific kind of a `Operation::MetaTypeOperation` instruction.
    Stored in the instruction's `extra_params` as `MetaParameters`. Each kind maps to a compile-time
    type operation lowered by the DVM backend to extern-C `comptime_*` calls.
*/
MAKE_STRINGIFYABLE_ENUM(compiler::mir, u32, MetaKind,
	CreateBox,
	CreateRef,
	CreateConst,
	CreatePtr,
	CreateManyPtr,
	CreateCPtr,
	CreateSlice,
	CreateTuple,
	CreateVariant,
	Eq,
	Neq,
	SizeOf,
	AlignOf
)

namespace compiler::mir {
	/**
	 * @brief BlockID is a temporary solution that should be replaced by
	 * proper BlockReference.
	 * It is like that for now, to avoid confusion with BlockRef used in mir_lowering and
	 * transformation between that BlockRef to this "BlockRef".
	 */
	STRONG_TYPEDEF_ID_DIRECT_CREATION(BlockID);
}

ID_STD_HASH(::compiler::mir::BlockID);

/**
 * @brief These are flag types attached to the MIRLocal
 * used to mark that we want some behaviour in the MIR passes or not on the variable.
 *
 * May also denote some properties of the variable, like whether it is a return value or not.
 */
MAKE_FLAG_TYPE(compiler::mir, LifetimeFlag, LifetimeFlags,
	/// We do not generate destructor for this local.
	/// Used by the return value temporary, as it's destructor would have to be after the return.
	NoDestructor,

	/// We do not add the `ScopeStart` and `ScopeEnd` flags for this local.
	/// Used for return value and parameters, as their scope is always valid in the function.
	NoScopeFlags,

	/// Mark this local as a return value temporary, which can be used by the pipeline to handle it differently.
	/// DVM treats the return value tmp differently.
	ReturnTmpValue,
	
	/// Mark this local as a condition temporary, which can be used by the pipeline to handle it differently.
	/// Not used.
	ConditionTmpValue,

	/// Do not run the use-before-initialization check 
	// and use-after-free check for this local.
	NoMoveStatusValidation
)

namespace compiler::mir {
	struct MIRValue;

	/**
	 * @brief Whether given operation is an operation that can (and has to be)
	 * the last operation in the block (i.e. be a terminator).
	 */
	bool isTerminating(Operation);

	struct MIRConstant final {
		ctv::CompileTimeValue value;
	};

	/**
	 * Represent a direct reference to a function linked to a HELIOS SymID.
	 */
	struct MIRFunctionLiteral final {
		helios::SymID helios_id;
	};

	STRONG_TYPEDEF_ID_DIRECT_CREATION(LocalID);

	/**
	 * @brief Description of a MIR Local variable, like a function argument or simply local
	 * variable.
	 * @note This structure should only be stored directly in MIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct MIRLocal final {
		LocalID id;

		/**
		 * HELIOS SymID of the local variable.
		 * Locals without a helios_id are locals created for temporary values.
		 */
		base::Optional<helios::SymID> helios_id;
		tsh::SymbolType<>             type;

		/**
		 * Lifetime scopes of this local.
		 * This is used to determine when the local is valid (i.e. "live") and when it should be
		 * destructed. The local is valid only in instructions for which the instruction scope is in
		 * the scope-sub-tree of the lifetime scope of the local.
		 *
		 * It is optional, because the scope is not always known during creation of local variable.
		 * One MIR Function is created the scopes should always be set.
		 *
		 * If this is set to no_lifetime_scope (defined in MIR Function), then the local will be
		 * ignored by lifetime analysis. This means that no destructors will be inserted for this
		 * local, no use-after-free verification, no lifetimes analysis will be performed.
		 * This possibility should only be used for simple
		 * values generated by the compiler itself, to go-around the problem of using the value
		 * after destructors should be called, e.g: in return statements, for if condition value.
		 */
		base::Optional<ScopeRef> scope;

		LifetimeFlags lifetime_flags{};
		/**
		 * If this local is a function parameter, this field contains the index of the parameter.
		 */
		base::Optional<u64> parameter_index;

	private:
		// @note: Constructing MIRLocal from helios_id
		// might work poorly for template/generic instantiations.

		/**
		 * @brief Constructor for a local variables with a HELIOS SymID.
		 */
		MIRLocal(
			LocalID id, helios::SymID helios_id, tsh::SymbolType<> type, LifetimeFlags lifetime_flags
		):
			  id(id),
			  helios_id(helios_id),
			  type(type),
			  lifetime_flags(lifetime_flags) {}

		/**
		 * @brief Constructor for a local variables with a HELIOS SymID that are the function
		 * parameters.
		 */
		MIRLocal(
			LocalID           id,
			helios::SymID     helios_id,
			tsh::SymbolType<> type,
			LifetimeFlags     lifetime_flags,
			u64               parameter_index
		):
			  id(id),
			  helios_id(helios_id),
			  type(type),
			  lifetime_flags(lifetime_flags),
			  parameter_index(parameter_index) {}

		/**
		 * @brief Constructor for temporary values.
		 */
		MIRLocal(LocalID id, tsh::SymbolType<> type): id(id), helios_id({}), type(type) {}

		friend struct Function;
		friend struct FunctionBuilder;
		friend MIRLocalRef;

	public:
		/**
		 * Setter of lifetime scope of this local.
		 * MIR lowering uses it to set the lifetime scope of the local
		 * during the process of generating function code from HOUT.
		 */
		void setLifetimeScope(ScopeRef scope);

		void debugPrint(std::ostream& os, bool detailed = false) const;

		[[nodiscard]]
		base::StrID getName() const;

		bool operator==(const MIRLocal& other) const { return id == other.id; }

		/**
		 * @brief Returns true if this local is not of a unit type or a similar data-less type.
		 */
		[[nodiscard]]
		bool carriesInformation(query::Context& ctx) const {
			return type.getType().carriesInformation(ctx);
		}

		[[nodiscard]]
		bool isTemporary() const {
			return helios_id.empty();
		}
	};

	/**
	 * @brief Lightweight ID-like representation of a global value used in MIR IR code (e.g. in
	 * MIRPlace). By global-value we refer to a global variable or a global constant.
	 *
	 * @important This is not a full IR representation of a global variable.
	 * MIRGlobalData serves that purpose and contains more information.
	 *
	 * This structure is used to reference a global variable in MIR code. It is directly connected
	 * to the value from HELIOS-ID of a global entity.
	 *
	 * @details
	 * - The `helios_id` field is the HELIOS SymID of the global variable, used for referencing.
	 * - The `type` field stores the type of the global variable.
	 *
	 * This structure enables MIR instructions to refer to and manipulate global variables that
	 * originate from HOUT global data.
	 */
	struct MIRGlobal final {
		enum class Kind { Variable, Constant };

		/**
		 * @brief HELIOS SymID of the global variable.
		 * It is used to reference the global variable in the code.
		 */
		helios::SymID helios_id;

		/**
		 * @brief Type of the global variable.
		 */
		tsh::SymbolType<> type;

		Kind kind;

		MIRGlobal(helios::SymID helios_id, tsh::SymbolType<> type, Kind kind):
			  helios_id(helios_id),
			  type(type),
			  kind(kind) {}

		void debugPrint(std::ostream& os, bool detailed = false) const;

		/**
		 * @brief Returns true if this local is not of a unit type or a similar data-less type.
		 */
		[[nodiscard]]
		bool carriesInformation(query::Context& ctx) const {
			return type.getType().carriesInformation(ctx);
		}
	};

	/**
	 * @brief Represents access into a variable (local or global), or its component.
	 *
	 * It consists of a base variable and a projection chain - field projections, index projections
	 * or deref projections (if any of the elements was a reference))
	 *
	 * For example:
	 * - For an access like `a.b.c`, where `a` is a local or global variable, and `b` and
	 * `c` are fields within that variable, this structure would contain the base variable (`a`) and
	 * the projection chain (`[FieldProjection(`b`), FieldProjection(`c`)]`).
	 *
	 * - If `a` was a reference type, the access expression `a.b.c` would contain the base variable
	 * (`a`) and the projection chain (`[DerefProjection, FieldProjection(`b`),
	 * FieldProjection(`c`)]`).
	 *
	 * - Additionally, if field `b` was a reference type, an additional
	 * `DerefProjection` would be inserted right after `FieldProjection(`b`).
	 *
	 * - In case of `class_array[ix].some_field` the projection chain would contain:
	 *     - An index projection with the `index` set to the MIRValue representing `ix`
	 *     - A deref projection since the `[]` returns a reference to the inner element.
	 *     - An field projection with `field_id` set to `some_field`
	 *
	 * - For access to the whole variable with a direct specifier (e.g., just `a`), the projection
	 * chain would be empty.
	 */
	struct MIRPlace final {
		struct DerefProjection {
			bool operator==(const DerefProjection&) const = default;
		};

		struct FieldProjection {
			helios::SymID field_id;
			bool          operator==(const FieldProjection&) const = default;
		};

		struct IndexProjection {
			// Box is needed because of the cyclic dependency:
			// IndexProjection -> MIRValue -> MIRPlace -> MIRValue.
			// We also want MIRPlace to be copyable, thus it's Shared.
			SharedBox<MIRValue> index;
			bool                operator==(const IndexProjection&) const = default;
		};

		/**
		 * @brief A single projection which transforms a MIRPlace. This includes dereferencing,
		 * field access and index access for array elements.
		 */
		struct Projection {
			std::variant<DerefProjection, FieldProjection, IndexProjection> storage;

			static Projection field(helios::SymID field_id) {
				return Projection(FieldProjection(field_id));
			}

			static Projection deref() { return Projection(DerefProjection()); }

			static Projection index(const MIRValue& index) {
				auto shared_index = base::makeSharedBox<MIRValue>(index);
				return Projection(IndexProjection{ std::move(shared_index) });
			}

			bool operator==(const Projection& other) const = default;
		};

		// MIR Locals are stored indirectly through MIRLocalRef because
		// they are owned by MIR Function, unlike MIR Globals.
		using BaseVariant = std::variant<MIRLocalRef, MIRGlobal>;

		/**
		 * @brief Base of the LIR place, either local or global variable.
		 */
		BaseVariant base;

		/**
		 * @brief Get the type of the base variable.
		 */
		tsh::SymbolType<> getBaseType() {
			variant_match(base) {
				variant_case(MIRLocalRef, local) { return local->type; }
				variant_case(MIRGlobal, global) { return global.type; }
			}
			CORE_UNREACHABLE();
		}

		template<class T>
		const T& getBase() const {
			return std::get<T>(base);
		}

		/**
		 * @brief Sequence of operations applied to the `base` to reach the target memory.
		 */
		std::vector<Projection> projection_chain;

		/**
		 * @brief The type of the final accessed field after applying all projections.
		 * @note If the projection chain is empty, this type will be equal to the base type. It
		 * may differ from the base type if the projection chain is not empty.
		 */
		tsh::SymbolType<> type;

		/**
		 * Construct a MIRPlace from a local or global variable.
		 * @param base The base of the MIRPlace, which is a local or global variable.
		 */
		template<typename T>
		explicit MIRPlace(T&& base) requires(std::is_constructible_v<BaseVariant, T>)
			  : base(std::forward<T>(base)), type(getBaseType()) {}

		/**
		 * @brief Extend the MIRPlace structure by adding a new FieldProjection to the projection
		 * chain. Panics is a FieldProjection is added on a non direct type.
		 * @note This projection requires the type of the whole projection chain to be a direct
		 * type. Which means a deref should be inserted in HOUT whenever a field is accessed through
		 * a reference.
		 *
		 * @param ctx The query context for type resolution.
		 * @param field The next field to access.
		 * @return The extended MIRPlace structure.
		 */
		MIRPlace withField(query::Context& ctx, helios::SymID field) const;

		/**
		 * @brief Adds a DerefProjection to the projection chain. Panics if dereferencing a direct
		 * type.
		 * @return The extended MIRPlace with a DerefProjection.
		 */
		[[nodiscard]] MIRPlace withDeref() const;

		/**
		 * @brief Adds an IndexProjection to the projection chain. Panics if trying to index into a
		 * non-array type.
		 * @return The extended MIRPlace with an IndexProjection.
		 */
		[[nodiscard]] MIRPlace withIndex(const MIRValue& index) const;

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<MIRLocalRef>(base);
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<MIRGlobal>(base);
		}

		[[nodiscard]]
		bool hasProjections() const {
			return !projection_chain.empty();
		}

		/**
		 * @brief Returns true if the accessed field is not of a unit type or other data-less type.
		 */
		[[nodiscard]]
		bool carriesInformation(query::Context& ctx) const {
			return type.getType().carriesInformation(ctx);
		}

		void debugPrint(std::ostream& os, bool detailed = false) const;
	};

	/**
	 * @brief Structure representing any MIR value.
	 */
	struct MIRValue final {
	private:
		using ValueType = std::variant<MIRConstant, MIRPlace, BlockID, MIRFunctionLiteral>;

		ValueType value;

	public:
		MIRValue(MIRConstant value): value(value) {}

		MIRValue(MIRLocalRef value): value(MIRPlace(value)) {}

		MIRValue(MIRLocalMutRef value): value(MIRPlace(value)) {}

		MIRValue(MIRGlobal value): value(MIRPlace(value)) {}

		MIRValue(MIRPlace value): value(value) {}

		MIRValue(BlockID value): value(value) {}

		MIRValue(MIRFunctionLiteral value): value(value) {}

		void debugPrint(std::ostream& os) const;

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
		}

		/**
		 * @brief Returns reference value of given type
		 * stored in MIRValue.
		 * Throws if value is not of given type.
		 * @tparam T
		 * @return const T&
		 */
		template<class T>
		const T& get() const {
			return std::get<T>(value);
		}

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<MIRPlace>(value) && std::get<MIRPlace>(value).isLocal();
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<MIRPlace>(value) && std::get<MIRPlace>(value).isGlobal();
		}

		[[nodiscard]]
		bool isConstant() const {
			return std::holds_alternative<MIRConstant>(value);
		}

		/**
		 * @brief Returns true if this value contains valuable information.
		 * Valuable information is either a reference to a block or function,
		 * or contains a value (local, global, or literal) that is not of
		 * unit or void type, or any other information-less type.
		 */
		[[nodiscard]]
		bool carriesInformation(query::Context& ctx) const {
			variant_match(value) {
				variant_case(MIRConstant, constant) {
					return !constant.value.has<ctv::CompileTimeValue::UnitCTV>();
				}
				variant_case(MIRPlace, access) { return access.carriesInformation(ctx); }
				variant_default { return true; }
			}
			CORE_UNREACHABLE();
		}
	};

	/**
	 * @brief Structure representing meta information about operation such as:
	 * * does operation construct some variable
	 * * does operation destruct some variable
	 * * does operation move some variable
	 */
	struct OperationFlag final {
		enum class Flag { ScopeStart, ScopeEnd, Construct, Reinit, Destruct, Move };
		Flag        flag;
		MIRLocalRef local;

		void debugPrint(std::ostream& os) const;
	};

	/**
	 * @brief Creates construct flag for given local.
	 */
	constexpr OperationFlag flagConstruct(MIRLocalRef local) {
		return { .flag = OperationFlag::Flag::Construct, .local = local };
	}

	/**
	 * @brief Creates reinit flag for given local.
	 *
	 * Emitted when an assignment stores a value into a whole local (no projections), as opposed to
	 * a declaration. Like @ref flagConstruct it marks the local as alive from this point on for
	 * move-state analysis.
	 */
	constexpr OperationFlag flagReinit(MIRLocalRef local) {
		return { .flag = OperationFlag::Flag::Reinit, .local = local };
	}

	/**
	 * @brief Creates destruct flag for given local.
	 */
	constexpr OperationFlag flagDestruct(MIRLocalRef local) {
		return { .flag = OperationFlag::Flag::Destruct, .local = local };
	}

	/**
	 * @brief Creates move flag for given local.
	 */
	constexpr OperationFlag flagMove(MIRLocalRef local) {
		return { .flag = OperationFlag::Flag::Move, .local = local };
	}

	/**
	 * @brief Used to inform that the instruction doesn't require any additional parameters.
	 */
	struct NoInstrParameters final {};

	struct CastParameters final {
		/**
		 * @brief The source type of the cast operation.
		 */
		tsh::SymbolType<> source_type;
		/**
		 * @brief The target type of the cast operation.
		 */
		tsh::SymbolType<> target_type;
	};

	/**
	 * @brief Parameters of the VariantConstruct and VariantTryProject operations: the variant
	 * alternative being constructed/projected. The index refers to the canonical order of
	 * the interned variant type's alternatives.
	 */
	struct VariantParameters final {
		usize             alternative_index;
		tsh::SymbolType<> alternative_type;
	};

	/**
	 * @brief Additional parameters for a `Operation::MetaTypeOperation` instruction, selecting
	 * which meta operation it is.
	 */
	struct MetaParameters final {
		MetaKind kind;
	};

	/**
	 * @brief Additional parameters for MIR instructions that depend on the operation type.
	 * For example, cast instruction needs to know
	 * from which type to which type it is casting.
	 */
	using InstrParameters
		= std::variant<NoInstrParameters, CastParameters, VariantParameters, MetaParameters>;

	/**
	 * @brief Single instruction of MIR code.
	 */
	struct Instruction final {
		Operation operation = Operation::Uninitialized;

		base::Optional<MIRPlace> output;

		std::vector<MIRValue> arguments;

		// construct, destruct, move.
		// the order is significant for the `ScopeStart` and `ScopeEnd` flags
		std::vector<OperationFlag> flags;

		InstrParameters extra_params{ NoInstrParameters{} };

		InstructionMetadata metadata;

		// @TODO: each Instruction should have source position reference

		/**
		 * @brief lifetime-scope of this instruction.
		 */
		ScopeRef scope;

		Instruction()                   = delete;
		Instruction(const Instruction&) = default;

		/**
		 * @todo this line produces -Wmaybe-uninitialized warning for some reason.
		 * fix it.
		 */
		Instruction(Instruction&&) = default;

		Instruction(
			const Operation            operation,
			base::Optional<MIRPlace>   output,
			std::vector<MIRValue>      arguments,
			std::vector<OperationFlag> flags,
			const ScopeRef             scope,
			InstrParameters            extra_parameters = NoInstrParameters{},
			InstructionMetadata        metadata         = {}
		):
			  operation(operation),
			  output(std::move(output)),
			  arguments(std::move(arguments)),
			  flags(std::move(flags)),
			  extra_params(extra_parameters),
			  metadata(metadata),
			  scope(scope) {}

		void debugPrint(std::ostream& os) const;
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
		 */
		Instruction terminator;

		[[nodiscard]]
		ScopeRef beginScope() const;

		/**
		 * @brief Returns the first instruction of the block.
		 * It may be the terminator instruction if the block is empty,
		 * but it always exists, because every block has to have a terminator instruction.
		 */
		[[nodiscard]] Instruction& firstInstruction();
	};

	/**
	 * @brief Helper struct for adding additional semantic information
	 * to the SymID.
	 * See: Function::HSymID for usage
	 */
	struct FunctionSymID final {
		helios::SymID id;
	};

	/**
	 * @brief Helper struct for adding additional semantic information
	 * to the SymID
	 * See: Function::HSymID for usage
	 */
	struct GlobalVariableCtorDtor final {
		enum class Type { Ctor, Dtor };

		Type          type;
		helios::SymID global_var_id;

		static GlobalVariableCtorDtor ctor(helios::SymID global_var_id) {
			return { .type = Type::Ctor, .global_var_id = global_var_id };
		}

		static GlobalVariableCtorDtor dtor(helios::SymID global_var_id) {
			return { .type = Type::Dtor, .global_var_id = global_var_id };
		}
	};

	/**
	 * @brief Function in MIR.
	 */
	struct Function final {
		/**
		 * This name is only used for debugging and error logging and is not mangled (and is not
		 * used for mangling in LIR)
		 */
		base::StrID name;

		tsh::SymbolType<>              return_type;
		std::vector<tsh::SymbolType<>> parameter_types;

		/**
		 * @brief Map from BlockID to the Block.
		 * The block's content is stored here.
		 * @note Block with ID "0" should always be the one with FunctionEnd (@p
		 * finalizeFunctionEnd)
		 */
		base::StableHashMap<BlockID, Block> blocks;

		/**
		 * @brief The generated order of blocks in the function.
		 * It serves as a list of all the blocks that are inside the function.
		 * The order is not important but it is more human friendly.
		 * First block in the block order is the entry block.
		 */
		std::vector<BlockID> block_order;

		/**
		 * @brief List of all local variables in the function.
		 */
		base::StableVector<const MIRLocal> local_list;

		/**
		 * Lifetimes scope-tree of this function.
		 */
		LifetimeScopeTree lifetime_scope_tree;

		/**
		 * Special scope for local variables that are not
		 * omitted by lifetime analysis and destructor calls.
		 * See MIRLocal::scope for details.
		 */
		ScopeRef no_lifetime_scope;

		/**
		 * HELIOS SymID related to the function.
		 * Functions without a helios_id are functions created for eg. from expressions
		 */
		using HSymID = std::variant<FunctionSymID, GlobalVariableCtorDtor>;

		HSymID helios_id;

		Function()                    = delete;
		Function(const Function&)     = delete;
		Function(Function&&) noexcept = default;

		Function& operator=(const Function&) = delete;

		// We can change it to default, when there will be a reason:
		Function& operator=(Function&&) = delete;

		Function(
			base::StrID                         name,
			tsh::SymbolType<>                   return_type,
			std::vector<tsh::SymbolType<>>      parameter_types,
			base::StableHashMap<BlockID, Block> blocks,
			std::vector<BlockID>                block_order,
			base::StableVector<const MIRLocal>  local_list,
			LifetimeScopeTree                   lifetime_scope_tree,
			ScopeRef                            no_lifetime_scope,
			HSymID                              helios_id
		);

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

		/**
		 * @brief Adds a compiler generated local to an already built function and returns it. Used
		 * when adding lifetime flags in `AddLifetimeFlagsPass`.
		 */
		MIRLocalRef addGeneratedLocal(tsh::SymbolType<> type, ScopeRef scope, LifetimeFlags flags);

		void debugPrint(std::ostream& os) const;

		/**
		 * @brief Checks if the id's from the HashMap match the id's in the blocks,
		 * if all block_order elements are present in the HashMap and
		 * if the jump targets exist.
		 * Used for debugging.

		 * @note If there is a block in the HashMap but not in the block_order,
		 * it is considered invalid.
		 */
		[[nodiscard]]
		base::OkBad validateBlockIDs() const;

		[[nodiscard]] BlockID lastBlock() const { return block_order.back(); }
	};

	struct MIRCtorDtorPair {
		CRef<mir::Function>                 constructor;
		base::Optional<CRef<mir::Function>> destructor;
	};

	/**
	 * @brief Representation of a global value in MIR.
	 * See also: MIRGlobal
	 */
	struct MIRGlobalData final {
		MIRGlobal global;

		/**
		 * @brief Initial value for the global variable.
		 * Can be either a compile-time value or a reference to a ctor/dtor functions.
		 * Should always be a CTV if kind is Const
		 */
		std::variant<ctv::CompileTimeValue, MIRCtorDtorPair> initial_value;

		void debugPrint(query::Context& ctx, std::ostream& out) const;
	};

	/**
	 * @brief Structure representing single MIRUnit.
	 *
	 * MIR unit is an arbitrary code collections represented in MIR IR.
	 * There is no assumption on what any given MIRUnit should contain.
	 *
	 * @note MIR units are created mostly from HOUT units.
	 */
	struct MIRUnit final {
		std::vector<CRef<mir::Function>> mir_functions;
		std::vector<CRef<MIRGlobalData>> mir_globals;

		void debugPrint(query::Context& ctx, std::ostream& out) const;
	};
}

ID_STD_HASH(compiler::mir::LocalID);

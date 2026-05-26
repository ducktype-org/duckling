#pragma once

#include "function_forward.hpp"  // IWYU pragma: keep

#include <ctv/ctv.hpp>
#include <diagnostic_interactive/stable_position.hpp>
#include <helios/hout/hout_fd.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_metadata.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ok_bad.hpp>

#include <query_framework/context/context_fd.hpp>

#include <memory>
#include <ostream>
#include <utility>

// Doc style is intentional, caused by inexplicable funkiness in how Doxygen interacts with macros.
MAKE_STRINGIFYABLE_ENUM(compiler::lir, u64, Operation,
	/** Placeholder for uninitialized value, should not be in LIR output. */
	Uninitialized,

	/** Simple byte by byte assignment. */
	Assign,
	AddressOf, 
	BoxAlloc,
	// @TODO: #1894 This approach (for both `BoxFree` and `ListFree`) may be temporary and 
	// depends on how we handle destructors in the future.
	BoxFree,
	ListFree,

	ListPush,
	ListPop,
	ListLen,

	/**
		@brief Placeholder.
		@todo Some decisions here to be made about operations like that.
		Perhaps we want more generic code for LIR, so algorithms are simpler.
		There could be single operation for all Add, Sub, etc, and single one for all comparisons.

		Some operations are sign-sensitive and are prefixed with U or S, e.g. UDiv and SDiv.
	*/

	/** Integer arithmetic. */
	IntegerAdd,
	IntegerSub,
	IntegerNeg,
	IntegerMul,
	IntegerUDiv,
	IntegerSDiv,
	IntegerUMod,
	IntegerSMod,

	/** Floating point arithmetic. */
	FloatAdd,
	FloatSub,
	FloatMul,
	FloatDiv,
	FloatNeg,

	/** Integer comparisons. */
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

	/** Floating point comparisons. */
	FloatLt,
	FloatGt,
	FloatLteq,
	FloatGteq,
	FloatEq,
	FloatNeq,

	/** Meta type operations. */
	MetaCreateBox,
	MetaCreateRef,
	MetaCreateConst,
	MetaCreateTuple, // N arguments, types to create the tuple type from
	MetaCreateVariant, // N arguments, types to create the variant type from
	MetaEq,
	MetaNeq,

	BooleanAnd,
	BooleanOr,
	BooleanNot,

	Cast,
	ZeroInitialize,

	Call,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch,
	
	// Nop can be useful when lowering the instruction flags and MIR instr translates
	// to zero instructions in LIR, but we want to have the flags in correct place.
	Nop
)

/// A helper tag that indicates that a value has some special meaning
/// that can be used by the lower layers.
MAKE_STRINGIFYABLE_ENUM(compiler::lir, u32, LIRLocalSpecialKind,
	/// Normal local.
	Normal,
	/// This is the return value temporary local.
	ReturnValue,
	/// This is a condition result temporary local.
	ConditionTmp
)

namespace compiler::lir {
	struct LIRLocal;
	struct LIRValue;
	struct Block;
	struct Function;

	/**
	 * @brief Reference to local variable in LIR.
	 */
	using LIRLocalRef = CRef<LIRLocal>;

	/**
	 * @brief Reference to block in LIR.
	 */
	using BlockRef = CRef<Block>;

	/**
	 * @brief Reference to a function in LIR.
	 */
	struct FunctionLiteral {
		base::StrID                                         mangled_name;
		helios::SymbolABI                                   abi;
		std::shared_ptr<std::vector<CRef<tsl::TypeLayout>>> parameter_layouts;
		CRef<tsl::TypeLayout>                               return_type_layout;

		static FunctionLiteral fromFunction(const Function&);
	};

	/**
	 * @brief Metadata for LIR local variables or function arguments.
	 * Used by the backends for the DebugInfo.
	 */
	struct LIRLocalMetadata {
		base::Optional<base::StrID>             source_code_name;
		base::Optional<dia_int::StablePosition> position;
	};

	LIRLocalSpecialKind specialKindFromMIR(const mir::MIRLocal& mir_local);

	/**
	 * @brief Description of a LIR Local variable or function argument.
	 * @note This structure should only be stored directly in LIR Function, as part of the
	 * description of a function. Other uses should use LocalRef to reference the variable
	 * description.
	 */
	struct LIRLocal final {
		/**
		 * @brief HELIOS id of the variable, if exists.
		 */
		base::Optional<helios::SymID> helios_id;

		CRef<tsl::TypeLayout> layout;

		/**
		 * @brief Index of the parameter in the function, if this is a function parameter.
		 */
		base::Optional<u64> parameter_index;

		LIRLocalMetadata metadata;

		LIRLocalSpecialKind special_kind;

	private:
		LIRLocal(
			const base::Optional<helios::SymID> helios_id,
			const CRef<tsl::TypeLayout>         layout,
			const base::Optional<u64>           parameter_index,
			LIRLocalMetadata                    metadata,
			LIRLocalSpecialKind                 special_kind = LIRLocalSpecialKind::Normal
		):
			  helios_id(helios_id),
			  layout(layout),
			  parameter_index(parameter_index),
			  metadata(metadata),
			  special_kind(special_kind) {}

		explicit LIRLocal(const CRef<tsl::TypeLayout> layout):
			  helios_id({}),
			  layout(layout),
			  special_kind(LIRLocalSpecialKind::Normal) {}

		friend Function;
		friend LIRLocalRef;

	public:
		/**
		 * @brief Creates LIR local data from MIR local data.
		 * @important remember that LIRLocal should only be stored in a LIR function.
		 * @note Do not use this function outside of LIR lowering.
		 *
		 * @param ctx
		 * @param mir_local
		 * @param new_parameter_index If the local is a parameter, this should be its index in the
		 * LIR function's parameter list. This is needed to adjust for discarded parameters with
		 * information-less types.
		 * @return LIRLocal
		 */
		static LIRLocal fromMIR(
			query::Context&     ctx,
			mir::MIRLocalRef    mir_local,
			base::Optional<u64> new_parameter_index = {}
		);

		/**
		 * @brief Crates unique local with bool-type, and without
		 * helios_id.
		 * @note it's used to create lifetime-flags
		 * @param ctx
		 * @return LIRLocal
		 */
		static LIRLocal boolLocal(query::Context& ctx);
	};

	enum class LIRGlobalType { Variable, Constant };

	/**
	 * @brief Global variable/constant in LIR.
	 */
	struct LIRGlobal final {
		// #2246 do something about this
		// see #1657
		// /**
		//  * @brief HELIOS id of the variable.
		//  * #2246 this should not be here, its lir!
		//  */
		// helios::SymID helios_id;

		CRef<tsl::TypeLayout> layout;

		base::StrID mangled_name;

		LIRGlobalType type;

		base::Optional<ctv::CompileTimeValue> initial_value;

	private:
		LIRGlobal(
			// const helios::SymID                          helios_id,
			const CRef<tsl::TypeLayout>                  layout,
			const base::StrID&                           mangled_name,
			const LIRGlobalType                          type          = LIRGlobalType::Variable,
			const base::Optional<ctv::CompileTimeValue>& initial_value = {}
		):
			//   helios_id(helios_id),
			  layout(layout),
			  mangled_name(mangled_name),
			  type(type),
			  initial_value(initial_value) {}

		friend Function;

	public:
		/**
		 * @note Do not use this function outside of LIR lowering.
		 */
		static LIRGlobal fromMIR(query::Context& ctx, mir::MIRGlobal mir_global);

		/**
		 * @note Do not use this function outside of LIR lowering / driver.
		 * This handles both global variables and constants. For constants, it also sets CTV initial
		 * value of the global.
		 */
		static LIRGlobal fromHOUT(query::Context& ctx, const helios::HOUTGlobalData& helios_id);


		void debugPrint(query::Context& ctx, std::ostream& os) const;
	};

	/**
	 * @brief Represents access into a variable (local or global), or its component.
	 *
	 * It contains of a base variable and a projection chain - field projections, index projections
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
	struct LIRPlace final {
		struct DerefProjection {
			bool operator==(const DerefProjection&) const = default;
		};

		struct FieldProjection {
			helios::SymID field_id;
			bool          operator==(const FieldProjection&) const = default;
		};

		struct IndexProjection {
			// Box is needed because of the cyclic dependency:
			// IndexProjection -> LIRValue -> LIRPlace -> LIRValue.
			// We also want MIRPlace to be copyable, thus it's Shared.
			SharedBox<LIRValue> index;
			bool                operator==(const IndexProjection&) const = default;
		};

		/**
		 * @brief A single projection which transforms a LIRPlace. This includes dereferencing,
		 * field access and index access for array elements.
		 */
		struct Projection {
			std::variant<DerefProjection, FieldProjection, IndexProjection> storage;

			static Projection field(helios::SymID field_id) {
				return Projection(FieldProjection(field_id));
			}

			static Projection deref() { return Projection(DerefProjection()); }

			static Projection index(const LIRValue& index) {
				auto index_shared = base::makeSharedBox<LIRValue>(index);
				return Projection(IndexProjection{ std::move(index_shared) });
			}

			bool operator==(const Projection& other) const = default;
		};

		using BaseVariant = std::variant<LIRLocalRef, LIRGlobal>;
		/**
		 * @brief Base of the LIR place, either local or global variable.
		 */
		BaseVariant base;

		/**
		 * @brief Get the type layout of the base variable.
		 */
		[[nodiscard]]
		CRef<tsl::TypeLayout> getBaseLayout() const {
			variant_match(base) {
				variant_case(LIRLocalRef, local) { return local->layout; }
				variant_case(LIRGlobal, global) { return global.layout; }
			}
			CORE_UNREACHABLE();
		}

		template<class T>
		const T& getBase() const {
			return std::get<T>(base);
		}

		/**
		 * @brief The type layout of the final accessed field after applying all projections.
		 * @note If the projection chain is empty, this layout will be equal to the `base` type
		 * layout. It may differ from the base layout if the projection chain is not empty.
		 */
		CRef<tsl::TypeLayout> layout;

		/**
		 * @brief Sequence of operations applied to the `base` to reach the target memory.
		 */
		std::vector<Projection> projection_chain;


		LIRPlace(BaseVariant base, std::vector<Projection> access_chain);

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<LIRLocalRef>(base);
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<LIRGlobal>(base);
		}

		[[nodiscard]]
		bool hasProjections() const {
			return !projection_chain.empty();
		}
	};

	/**
	 * @brief Representation of a constant known at compile time.
	 */
	struct LIRConstant final {
		ctv::CompileTimeValue value;
		CRef<tsl::TypeLayout> layout;
	};

	/**
	 * @brief Any value in LIR representation
	 */
	struct LIRValue {
	private:
		using ValueType = std::variant<LIRConstant, LIRPlace, BlockRef, FunctionLiteral>;
		ValueType value;

	public:
		LIRValue(LIRConstant value): value(value) {}

		LIRValue(LIRPlace value): value(value) {}

		LIRValue(BlockRef value): value(value) {}

		LIRValue(FunctionLiteral value): value(value) {}

		[[nodiscard]]
		const ValueType& getVariant() const {
			return value;
		}

		[[nodiscard]]
		bool isLocal() const {
			return std::holds_alternative<LIRPlace>(value) && std::get<LIRPlace>(value).isLocal();
		}

		[[nodiscard]]
		bool isGlobal() const {
			return std::holds_alternative<LIRPlace>(value) && std::get<LIRPlace>(value).isGlobal();
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

		/**
		 * @brief Whether a LIRValue holds a type T.
		 */
		template<class T>
		[[nodiscard]] bool is() const {
			return std::holds_alternative<T>(value);
		}
	};

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
		/**
		 * @brief The source type layout of the cast operation.
		 */
		CRef<tsl::TypeLayout> source_layout;
		/**
		 * @brief The target type layout of the cast operation.
		 */
		CRef<tsl::TypeLayout> target_layout;
	};

	struct ListOperationParameters final {
		/**
		 * @brief The element layout for generic `ListPush` and `ListPop` operations.
		 */
		CRef<tsl::TypeLayout> element_layout;
	};

	/**
	 * @brief Additional parameters for LIR instructions that depend on the operation type.
	 */
	using InstrParameters
		= std::variant<NoInstrParameters, CastParameters, ListOperationParameters>;

	struct InstructionMetadata {
		base::Optional<dia_int::StablePosition> position;

		InstructionMetadata(const mir::InstructionMetadata& other): position(other.position) {}

		InstructionMetadata() = default;
	};

	/**
	 * @brief The scope flags of the instruction.
	 * It's a flag that indicates the start and end of a variable's scope,
	 * for efficient local stack allocation in the backend.
	 * The only user is for now the DVM Backend, which uses it to generate `init` and `deinit`
	 * instructions.
	 */
	struct ScopeFlag {
		enum class Flag { ScopeStart, ScopeEnd };
		Flag        flag;
		LIRLocalRef local;

		bool operator==(const ScopeFlag& other) const = default;
	};

	/**
	 * @brief Single instruction of LIR code.
	 */
	struct Instruction final {
		Operation                operation = Operation::Uninitialized;
		base::Optional<LIRPlace> output;
		std::vector<LIRValue>    arguments;
		InstrParameters          extra_params{ NoInstrParameters{} };
		InstructionMetadata      metadata;

		std::vector<ScopeFlag> scope_flags;


		Instruction()                       = default;
		Instruction(const Instruction&)     = default;
		Instruction(Instruction&&) noexcept = default;

		Instruction& operator=(Instruction&&) noexcept = default;

		Instruction(
			const Operation          operation,
			base::Optional<LIRPlace> output,
			std::vector<LIRValue>    arguments,
			InstructionMetadata      metadata,
			InstrParameters          extra_parameters = NoInstrParameters{}
		):
			  operation(operation),
			  output(std::move(output)),
			  arguments(std::move(arguments)),
			  extra_params(extra_parameters),
			  metadata(metadata) {}

		/**
		 * Whether the instruction can be the last instruction in the block (i.e. be a terminator).
		 */
		[[nodiscard]] bool isTerminating() const;
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

	struct FunctionMetadata {
		base::Optional<dia_int::StablePosition> position;
		base::Optional<base::StrID>             source_code_name;
	};

	/**
	 * @brief Function in LIR.
	 */
	struct Function final {
		base::StrID       mangled_name;
		helios::SymbolABI abi;

		std::vector<CRef<tsl::TypeLayout>> parameter_layouts;
		CRef<tsl::TypeLayout>              return_type_layout;

		base::StableVector<Block>    blocks;
		base::StableVector<LIRLocal> local_list;

		std::vector<BlockRef> block_order;

		FunctionMetadata metadata;

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
		 * @note Those ids do not carry any meaning, they are made here to be consistent in
		 * different part of compiler (e.g. lir printing, llvm lowering).
		 * @return base::Map<BlockRef, u64>
		 */
		[[nodiscard]]
		base::Map<LIRLocalRef, u64> getLocalVariableIDs() const;
	};

	/**
	 * @brief Structure representing single LIRUnit.
	 *
	 * LIR unit is an arbitrary code collections represented in LIR IR.
	 * There is no constract on what any given LIRUnit should contain.
	 * 
	 * @note LIR units are created mostly from MIR units.
	 */
	struct LIRUnit final {
		std::vector<CRef<Function>> lir_functions;
		std::vector<LIRGlobal>      lir_globals;
	};
}

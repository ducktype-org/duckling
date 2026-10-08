// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lir_structure_fd.hpp"  // IWYU pragma: keep

#include <abi/calling_conv/calling_conv.hpp>
#include <ctv/ctv.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/hout_fd.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>  // @TODO: #2796 untable this if possible (LIR structure should not depend on symbols if possible)
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_metadata.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/types/ok_bad.hpp>

#include <diagnostic/stable_position.hpp>
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
	
	/** Integer bitwise operations. */
	IntegerBitAnd,
	IntegerBitOr,
	IntegerBitXor,
	IntegerBitNot,
	IntegerShl,
	IntegerShr,

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

	/**
	 * Meta type operation. The specific operation is parametrized by `MetaParameters` (a
	 * `MetaKind`) stored in the instruction's `extra_params`.
	 */
	MetaTypeOperation,

	BooleanAnd,
	BooleanOr,
	BooleanNot,

	Cast,
	ZeroInitialize,

	/** Creates a variant value from a payload value (see mir::Operation::VariantConstruct). */
	VariantConstruct,
	/**
	 * Pointer to the variant's payload, null on alternative mismatch. Its single argument is a
	 * reference to the variant, not the variant place itself.
	 */
	VariantTryProject,

	Call,

	ReturnVoid,
	ReturnValue,
	Jump,
	Branch,
	/** Terminator: [pointer, null_target, not_null_target]. */
	BranchIfNull,

	/** Terminator: marks control flow that can never be reached (e.g. after a diverging call). */
	Unreachable,

	// Nop can be useful when lowering the instruction flags and MIR instr translates
	// to zero instructions in LIR, but we want to have the flags in correct place.
	Nop
)

/**
 *   @brief The specific kind of a `Operation::MetaTypeOperation` instruction.
 *   Stored in the instruction's `extra_params` as `MetaParameters`. Mirrors `mir::MetaKind`.
 */
MAKE_STRINGIFYABLE_ENUM(compiler::lir, u32, MetaKind,
	CreateBox,
	CreateRef,
	CreateConst,
	CreatePtr,
	CreateManyPtr,
	CreateCPtr,
	CreateSlice,
	CreateTuple,
	CreateVariant,
	CreateOptional,
	Eq,
	Neq,
	SizeOf,
	AlignOf
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

	struct LIRAbi final {
		struct CAbi final {
			abi::calling_conv::FunctionInfo function_info;
		};

		struct DefaultAbi final {};

		using ValueType = std::variant<CAbi, DefaultAbi>;
		ValueType value;
	};

	/**
	 * @brief Function which call will be replaced
	 * manually in the backend.
	 */
	enum class BuiltinFunctionKind {
		DvmAllocArr,
		DvmReallocArr,
		DvmFreeArr,
		DvmAlloc,
		DvmFree,
		DvmPtrParts,
		DvmIsNullptr,
		DvmNullptr
	};

	base::Optional<BuiltinFunctionKind> getBuiltinKindFromHOUT(helios::BuiltinKind kind);

	/**
	 * @brief Reference to a function in LIR.
	 */
	struct FunctionLiteral final {
		base::StrID                                         mangled_name;
		LIRAbi                                              abi;
		bool                                                link_once;
		std::shared_ptr<std::vector<CRef<tsl::TypeLayout>>> parameter_layouts;
		CRef<tsl::TypeLayout>                               return_type_layout;

		/**
		 * Optional indicates if a function literal is a builtin function.
		 * Empty value indicates that a function is not a builtin.
		 */
		base::Optional<BuiltinFunctionKind> builtin_kind_opt;

		static FunctionLiteral fromFunction(const Function&);
	};

	/**
	 * @brief Metadata for LIR local variables or function arguments.
	 * Used by the backends for the DebugInfo.
	 */
	struct LIRLocalMetadata {
		base::Optional<base::StrID>         source_code_name;
		base::Optional<dia::StablePosition> position;
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
		 * @brief Creates unique local holding a reference to @p pointee_type, and without
		 * helios_id.
		 * @note It's used to materialize addresses of places passed to functions taking
		 * references.
		 * @param ctx
		 * @param pointee_type Type of the referenced value.
		 * @return LIRLocal
		 */
		static LIRLocal refLocal(query::Context& ctx, tsh::SymbolType<> pointee_type);
	};

	enum class LIRGlobalType { Variable, Constant };

	/**
	 * @brief Lightweight ID-like representation of a global value used in LIR IR code (e.g. in
	 * LIRPlace). By global-value we refer to a global variable or a global constant.
	 *
	 * @important This is not a full IR representation of a global variable.
	 * LIRGlobalData serves that purpose and contains more information.
	 *
	 * This structure is used to reference a global variable in LIR code.
	 * This structure enables LIR instructions to refer to and manipulate global variables and
	 * constants.
	 */
	struct LIRGlobal final {
		CRef<tsl::TypeLayout> layout;

		base::StrID mangled_name;

		LIRGlobalType type;

		/**
		 * @brief Whether the global is replicated into every module that uses it (e.g. a constant
		 * belonging to a template instance), so its definition must be merged at link time.
		 */
		bool link_once;

	private:
		LIRGlobal(
			const CRef<tsl::TypeLayout> layout,
			const base::StrID&          mangled_name,
			const LIRGlobalType         type,
			const bool                  link_once
		):
			  layout(layout),
			  mangled_name(mangled_name),
			  type(type),
			  link_once(link_once) {}

		friend Function;

	public:
		/**
		 * @note Do not use this function outside of LIR lowering.
		 */
		static LIRGlobal fromMIR(query::Context& ctx, mir::MIRGlobal mir_global);

		void debugPrint(std::ostream& output, base::Optional<Ref<query::Context>> ctx = {}) const;
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


		/**
		 * @brief Construct a place and compute the layout it ends at.
		 * @param ctx The query::Context used to look up the layouts of dereferenced pointees.
		 * @param base The base variable of the place.
		 * @param access_chain The projections applied to the base.
		 */
		LIRPlace(query::Context& ctx, BaseVariant base, std::vector<Projection> access_chain);

		/**
		 * @brief Construct a place without projections, which ends at the layout of its base.
		 * @param base The base variable of the place.
		 */
		explicit LIRPlace(BaseVariant base);

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

		/**
		 * Prints this place, assigning IDs to referenced locals in encounter order
		 * or using local and block IDs from the function if given.
		 */
		void debugPrint(std::ostream& output, base::Optional<CRef<Function>> function = {}) const;
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

		LIRValue(FunctionLiteral value): value(std::move(value)) {}

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

		/**
		 * Prints this value, assigning IDs to referenced locals and blocks in encounter order
		 * or using local and block IDs from the function if given.
		 */
		void debugPrint(std::ostream& output, base::Optional<CRef<Function>> function = {}) const;
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

	/**
	 * @brief Parameters of VariantConstruct/VariantTryProject: the variant alternative
	 * (index in the canonical order of the interned variant type) and its layout.
	 */
	struct VariantParameters final {
		usize                 alternative_index;
		tsh::SymbolType<>     alternative_type;
		CRef<tsl::TypeLayout> alternative_layout;
		CRef<tsl::TypeLayout> variant_layout;
	};

	/**
	 * @brief Additional parameters for a `Operation::MetaTypeOperation` instruction, selecting
	 * which meta operation it is.
	 */
	struct MetaParameters final {
		MetaKind kind;
	};

	/**
	 * @brief Additional parameters for LIR instructions that depend on the operation type.
	 */
	using InstrParameters
		= std::variant<NoInstrParameters, CastParameters, VariantParameters, MetaParameters>;

	struct InstructionMetadata {
		base::Optional<dia::StablePosition> position;

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

		/**
		 * Prints this instruction, assigning IDs to referenced locals and blocks in encounter order
		 * or using local and block IDs from the function if given.
		 */
		void debugPrint(std::ostream& output, base::Optional<CRef<Function>> function = {}) const;
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
		base::Optional<dia::StablePosition> position;
		base::Optional<base::StrID>         source_code_name;
	};

	/**
	 * @brief Function in LIR.
	 */
	struct Function final {
		base::StrID mangled_name;
		LIRAbi      abi;

		/**
		 * If true, this function can have repeated definitions
		 * across many modules and has weak linkage.
		 * Used when having identical function in many modules.
		 */
		bool link_once;

		/**
		 * If true, this function is not added to the module
		 * on the DVM backend, even if explicitely requested.
		 * Used by the DVM/native conditional compilation.
		 */
		bool ignore_on_dvm;

		/**
		 * If true, this function is not added to the module
		 * on the LLVM backend, even if explicitely requested.
		 * Used by the DVM/native conditional compilation.
		 */
		bool ignore_on_llvm;

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

		void debugPrint(std::ostream& output, base::Optional<Ref<query::Context>> ctx = {}) const;

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
	 * @brief Representation of a global value in LIR (i.e. a global variable or constant).
	 * See also: LIRGlobal
	 */
	struct LIRGlobalData final {
		struct CTorDtorPair final {
			base::Optional<CRef<lir::Function>>
				global_ctor;  ///< Optional, if the global has a constructor.
			base::Optional<CRef<lir::Function>>
				global_dtor;  ///< Optional, if the global has a destructor.
		};

		LIRGlobal                                         global;
		std::variant<ctv::CompileTimeValue, CTorDtorPair> data_initialization;

		/**
		 * @brief Returns the constructor and destructor pair for the global.
		 * Panics if the global does not have a constructor+destructor initialization.
		 * Use only when you are sure that the global has constructor+destructor initialization (or
		 * in tests).
		 *
		 * @return CTorDtorPair
		 */
		[[nodiscard]]
		CTorDtorPair getCtorDtorPair() const;

		/**
		 * @brief Returns the constant value for the global.
		 * Panics if the global does not have a constant initialization.
		 * Use only when you are sure that the global has a constant initialization (or
		 * in tests).
		 *
		 * @return ctv::CompileTimeValue
		 */
		[[nodiscard]]
		ctv::CompileTimeValue getConstValue() const;

		void debugPrint(std::ostream& output, base::Optional<Ref<query::Context>> ctx = {}) const;
	};

	/**
	 * @brief Structure representing single LIRUnit.
	 *
	 * LIR unit is an arbitrary code collections represented in LIR IR.
	 * There is no assumption on what any given LIRUnit should contain.
	 *
	 * @note LIR units are created mostly from MIR units.
	 */
	struct LIRUnit final {
		std::vector<CRef<Function>> lir_functions;
		std::vector<LIRGlobalData>  lir_globals;

		void debugPrint(std::ostream& output, base::Optional<Ref<query::Context>> ctx = {}) const;

		/**
		 * @brief Removes duplicate functions and globals from the LIR unit.
		 * (based on their mangled names).
		 *
		 * @note This is needed to handle the case of multiple script modules in the REPL, which
		 * might contain duplicated functions and globals.
		 * @TODO: #2694 #2424 Come back to this and maybe remove or adapt this method accordingly.
		 *
		 * @note In this context, don't use this method outside of REPL script compilation, as it
		 * might hide other issues with duplicated functions and globals in LIR units (unless there
		 * are good reasons to do so). It is only placed here to avoid potential logic duplication
		 * should we ever need to handle duplicated functions and globals in LIR units in other
		 * contexts.
		 */
		void deduplicateSymbols();
	};
}

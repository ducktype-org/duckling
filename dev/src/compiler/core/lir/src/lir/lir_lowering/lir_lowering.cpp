/**
 * @file lir_lowering.cpp
 * @brief File implementing process of creating LIR function from MIR function
 *
 * @note when adding new cases to logic in this file you should most likely edit:
 * - mir2lirOperation -- for new operations
 * - MIR2LIR::getLocation -- for new location
 * - MIR2LIR::lowerFlags -- for flag handling
 * - MIR2LIR::lowerInstruction -- for new instructions
 * - MIR2LIR::lowerTerminator -- for new terminators
 * - LowerToLIRFunction::provide -- for some new steps
 */

#include "lir_lowering.hpp"

#include "../lir_structure/lir_structure.hpp"

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <tsl/queries.hpp>
#include <tsl/type_layout.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <logger/logger.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <type_traits>
#include <utility>

// @opt: make switch-cases in this file "sorted"

namespace compiler::lir {
	/**
	 * @brief Mutable reference block in LIR.
	 */
	using MutBlockRef = Ref<Block>;

	u64 KeyOf_LowerToLIRFunction::queryUnstablePerfectHash() const {
		return function->queryUnstablePerfectHash();
	}

	/**
	 * @brief Converts the MIR function type to a LIR layout. Changes the return type to Unit if the
	 * function returns a type which doesn't carry information (e.g. for class/array constructors
	 * which don't carry information).
	 */
	CRef<tsl::TypeLayout> mirReturnType2LirLayout(query::Context& ctx, tsh::SymbolType<> type) {
		if (type.getType().carriesInformation(ctx))
			return &ctx.query<tsl::QuerySymbolTypeLayout>(type)->valueOrPanicMsg(
				"layout query failed at LIR stage"
			);
		else
			// Change the return type to Unit if the function returns a type which doesn't carry
			// information (e.g. for class/array constructors which don't carry information).
			return &ctx.query<tsl::QueryAbstractTypeLayout>(tsh::getUnitType())
			            ->valueOrPanicMsg("layout query failed at LIR stage");
	}

	FunctionLiteral getFunctionLiteralfromHELIOSID(query::Context& ctx, helios::SymID helios_id) {
		tsh::FunctionAbstractType type
			= ctx.query<helios::QueryTypeOfSymbol>(helios_id)
		          ->valueOrPanicMsg("Handling errors in MIR is not supported yet")
		          .getType();

		auto symbol_abi = ctx.query<helios::QuerySymbolABI>(helios_id)->valueOrPanicMsg(
			"Handling errors in MIR is not supported yet"
		);
		auto link_once    = helios::shouldLinkOnce(helios_id);
		auto mangled_name = helios::mangler::getSimpleMangledName(ctx, helios_id);

		auto return_type = mirReturnType2LirLayout(ctx, type.getResultType());
		std::vector<CRef<tsl::TypeLayout>> parameter_types;
		parameter_types.reserve(type.getParameterTypes().size());
		for (const auto& param: type.getParameterTypes())
			// Discard information-less parameters from LIR function parameter lists.
			if (param.getType().carriesInformation(ctx))
				parameter_types.emplace_back(
					&ctx.query<tsl::QuerySymbolTypeLayout>(param)->valueOrPanicMsg(
						"layout query failed at LIR stage"
					)
				);

		return FunctionLiteral{
			.mangled_name = mangled_name,
			.abi          = symbol_abi,
			.link_once    = link_once,
			.parameter_layouts
			= std::make_shared<std::vector<CRef<tsl::TypeLayout>>>(std::move(parameter_types)),
			.return_type_layout = return_type,
		};
	}

	/**
	 * @brief Maps MIR operation to LIR operation for those
	 * that have direct counterpart.
	 *
	 * @param mir_operation The MIR operation to convert.
	 * @param signed_version Whether to use a signed version of the operation, if relevant.
	 * @return Operation
	 */
	Operation mir2lirOperation(const mir::Operation mir_operation, const bool signed_version) {
		switch (mir_operation) {
		// Variable Assignment
		case mir::Operation::Assign:
			return Operation::Assign;
		case mir::Operation::AddressOf:
			return Operation::AddressOf;
		case mir::Operation::BoxAlloc:
			return Operation::BoxAlloc;
		case mir::Operation::ListPush:
			return Operation::ListPush;
		case mir::Operation::ListPop:
			return Operation::ListPop;
		case mir::Operation::ListLen:
			return Operation::ListLen;
		case mir::Operation::ZeroInitialize:
			return Operation::ZeroInitialize;

		// Control Flow
		case mir::Operation::ReturnValue:
			return Operation::ReturnValue;
		case mir::Operation::ReturnVoid:
			return Operation::ReturnVoid;
		case mir::Operation::Jump:
			return Operation::Jump;
		case mir::Operation::Branch:
			return Operation::Branch;

		// Integer Arithmetic
		case mir::Operation::IntegerAdd:
			return Operation::IntegerAdd;
		case mir::Operation::IntegerSub:
			return Operation::IntegerSub;
		case mir::Operation::IntegerMul:
			return Operation::IntegerMul;
		case mir::Operation::IntegerDiv:
			return signed_version ? Operation::IntegerSDiv : Operation::IntegerUDiv;
		case mir::Operation::IntegerMod:
			return signed_version ? Operation::IntegerSMod : Operation::IntegerUMod;
		case mir::Operation::IntegerNeg:
			return Operation::IntegerNeg;

		// Integer Comparison
		case mir::Operation::IntegerLt:
			return signed_version ? Operation::IntegerSLt : Operation::IntegerULt;
		case mir::Operation::IntegerGt:
			return signed_version ? Operation::IntegerSGt : Operation::IntegerUGt;
		case mir::Operation::IntegerLteq:
			return signed_version ? Operation::IntegerSLteq : Operation::IntegerULteq;
		case mir::Operation::IntegerGteq:
			return signed_version ? Operation::IntegerSGteq : Operation::IntegerUGteq;
		case mir::Operation::IntegerEq:
			return Operation::IntegerEq;
		case mir::Operation::IntegerNeq:
			return Operation::IntegerNeq;

		/// Floating point arithmetic ///
		case mir::Operation::FloatAdd:
			return Operation::FloatAdd;
		case mir::Operation::FloatSub:
			return Operation::FloatSub;
		case mir::Operation::FloatMul:
			return Operation::FloatMul;
		case mir::Operation::FloatDiv:
			return Operation::FloatDiv;
		case mir::Operation::FloatNeg:
			return Operation::FloatNeg;

		/// Floating point comparisons ///
		case mir::Operation::FloatLt:
			return Operation::FloatLt;
		case mir::Operation::FloatGt:
			return Operation::FloatGt;
		case mir::Operation::FloatLteq:
			return Operation::FloatLteq;
		case mir::Operation::FloatGteq:
			return Operation::FloatGteq;
		case mir::Operation::FloatEq:
			return Operation::FloatEq;
		case mir::Operation::FloatNeq:
			return Operation::FloatNeq;

		/// Meta type operations ///
		case mir::Operation::MetaCreateBox:
			return Operation::MetaCreateBox;
		case mir::Operation::MetaCreateRef:
			return Operation::MetaCreateRef;
		case mir::Operation::MetaCreateConst:
			return Operation::MetaCreateConst;
		case mir::Operation::MetaCreateTuple:
			return Operation::MetaCreateTuple;
		case mir::Operation::MetaCreateVariant:
			return Operation::MetaCreateVariant;
		case mir::Operation::MetaEq:
			return Operation::MetaEq;
		case mir::Operation::MetaNeq:
			return Operation::MetaNeq;

		/// Logic ///
		case mir::Operation::BooleanAnd:
			return Operation::BooleanAnd;
		case mir::Operation::BooleanOr:
			return Operation::BooleanOr;
		case mir::Operation::BooleanNot:
			return Operation::BooleanNot;
		// @TODO: add more cases
		default:
			CORE_PANIC(base::strConcat(
				"Operation without direct counterpart", base::enumToStr(mir_operation)
			));
		}
	}

	struct IMPLEMENT_QUERY(LowerToLIRFunction, Function) {
		/**
		 * @brief Helper struct for easy state encapsulation used.
		 * by LowerToLIRFunction query.
		 * @note This is not a typical struct, and
		 * should be seen as a set of functions operating on some common state.
		 * Order of those functions matter, as they build components of LIR function
		 * step by step.
		 */
		struct MIR2LIR final {
			Context& ctx;
			QKey     key;

			MIR2LIR(Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::StableVector<LIRLocal> locals;

			// locals mapping:
			base::Map<mir::MIRLocalRef, LIRLocalRef> mir_to_lir_local;
			base::Map<mir::MIRLocalRef, LIRLocalRef> mir_to_lifetime_flag;

			base::StableVector<Block> blocks;

			// blocks mapping:
			base::Map<mir::BlockID, MutBlockRef> mir_to_lir_block;

			std::vector<BlockRef> block_order;

			// helper functions:

			/**
			 * @brief Returns LIR local associated with given MIR local.
			 *
			 * @param mir_local
			 * @return LocalRef
			 */
			[[nodiscard]]
			LIRLocalRef getLocal(const mir::MIRLocalRef mir_local) const {
				return mir_to_lir_local.at(mir_local);
			}

			[[nodiscard]]
			LIRGlobal getGlobal(const mir::MIRGlobal& mir_global) const {
				return LIRGlobal::fromMIR(ctx, mir_global);
			}

			[[nodiscard]]
			LIRPlace getPlace(const mir::MIRPlace& mir_place) const {
				std::vector<LIRPlace::Projection> lir_projection_chain;
				lir_projection_chain.reserve(mir_place.projection_chain.size());

				// Map all MIR projections to LIR projections.
				for (const auto& proj: mir_place.projection_chain) {
					variant_match(proj.storage) {
						variant_case(mir::MIRPlace::FieldProjection, field) {
							lir_projection_chain.push_back(
								LIRPlace::Projection::field(field.field_id)
							);
						}
						variant_case(mir::MIRPlace::IndexProjection, index) {
							auto maybe_lir_index = getLocation(*index.index);
							CORE_ASSERT(
								maybe_lir_index.has_value(),
								"Array index must carry information (cannot be Unit/Void)"
							);

							lir_projection_chain.push_back(
								LIRPlace::Projection::index(maybe_lir_index.value())
							);
						}
						variant_case_novalue(mir::MIRPlace::DerefProjection) {
							lir_projection_chain.push_back(LIRPlace::Projection::deref());
						}
					}
				}

				variant_match(mir_place.base) {
					variant_case(mir::MIRLocalRef, local) {
						return { getLocal(local), std::move(lir_projection_chain) };
					}
					variant_case(mir::MIRGlobal, global) {
						return { getGlobal(global), std::move(lir_projection_chain) };
					}
				}
				CORE_UNREACHABLE();
			}

			/**
			 * @brief Get the output LIR location for the given MIR location. Returns an empty
			 * optional if the provided MIR location is empty, or if it does not carry information.
			 * @param output The MIR location to convert, possibly empty.
			 * @return The corresponding LIR location, possibly empty.
			 */
			[[nodiscard]]
			base::Optional<LIRPlace> getOutput(const base::Optional<mir::MIRPlace>& output) const {
				if (!output.has_value()) return {};
				if (!output->carriesInformation(ctx)) return {};
				return getPlace(*output);
			}

			/**
			 * @brief Maps a single MIR location to an optional LIR location.
			 * @note Discards information-less locations, e.g. variables of unit type.
			 * @param loc The MIR location.
			 * @return The optional LIR location, possibly discarded.
			 */
			[[nodiscard]] base::Optional<LIRValue> getLocation(const mir::MIRValue& loc) const {
				// Discard information-less location.
				if (!loc.carriesInformation(ctx)) return {};

				variant_match(loc.getVariant()) {
					variant_case(mir::MIRConstant, value) {
						if (value.value.has<ctv::CompileTimeValue::UnitCTV>())
							CORE_PANIC("Cannot get location of MIR unit.");

						auto layout = CRef<tsl::TypeLayout>(
							&ctx.query<tsl::QuerySymbolTypeLayout>(
									value.value.getTypeOfStoredValue(ctx)
							)
								 ->valueOrPanicMsg("layout query failed at LIR stage")
						);
						return LIRValue{ LIRConstant{ .value = value.value, .layout = layout } };
					}
					variant_case(mir::MIRPlace, place) { return LIRValue{ getPlace(place) }; }
					variant_case(mir::BlockID, block) {
						return LIRValue{ BlockRef(mir_to_lir_block.at(block)) };
					}
					variant_case(mir::MIRFunctionLiteral, func) {
						return getFunctionLiteralfromHELIOSID(ctx, func.helios_id);
					}
				}

				CORE_PANIC("Unhandled variant in getLocation");
			}

			// main functions:

			/**
			 * @brief Creates local vars data,
			 * puts them in a stable vector, and
			 * maps MIR locals to LIR local refs.
			 */
			void makeLocals() {
				// Mapping of MIR parameters to LIR parameters, accounting for empty param layouts.
				std::vector<base::Optional<usize>> mir_to_lir_parameter_indices;
				{
					usize lir_param_index = 0;
					for (const auto& mir_param_type: key.function->parameter_types)
						if (!mir_param_type.getType().carriesInformation(ctx))
							mir_to_lir_parameter_indices.emplace_back();
						else
							mir_to_lir_parameter_indices.emplace_back(lir_param_index++);
				}

				for (const auto& mir_local: key.function->local_list) {
					// Discard data-less variables.
					if (!mir_local.carriesInformation(ctx)) continue;

					auto lir_local = LIRLocal::fromMIR(
						ctx,
						&mir_local,
						mir_local.parameter_index.flatMap(
							[&mir_to_lir_parameter_indices](const usize mir_index) {
								return mir_to_lir_parameter_indices[mir_index];
							}
						)
					);

					locals.pushBack(lir_local);
					auto local_index = locals.lastIndex();
					mir_to_lir_local.put(&mir_local, locals[local_index]);

					// Only create lifetime flag if needed
					if (!mir_local.type.hasNoOpDestructor()) {
						auto lifetime_flag = LIRLocal::boolLocal(ctx);
						locals.pushBack(lifetime_flag);
						auto flag_index = locals.lastIndex();
						mir_to_lifetime_flag.put(&mir_local, locals[flag_index]);
					}
				}
			}

			void makeInitialBlocks() {
				// make initial block mapping, and
				// unfilled blocks that will map to
				// beginning of each mir block
				for (const auto& mir_block_id: key.function->block_order) {
					// note that this block will only be filled with instructions
					// and terminator later:
					blocks.pushBack({});
					mir_to_lir_block.put(mir_block_id, blocks.last());
				}
			}

			void lowerBlocks() {
				for (const auto& mir_block_id: key.function->block_order) {
					const auto lir_block = mir_to_lir_block[mir_block_id];
					block_order.emplace_back(lir_block);

					auto        curr_block = lir_block;
					const auto& mir_block  = key.function->blocks[mir_block_id];
					for (const auto& mir_instruction: mir_block.instructions)
						curr_block = lowerInstruction(curr_block, mir_instruction);

					lowerTerminator(curr_block, mir_block.terminator);
				}
			}

			// instruction lowering functions:

			/**
			 * @brief Maps list of MIR locations to LIR locations.
			 * @note Discards information-less locations, e.g. variables of unit type.
			 *
			 * @param locs The MIR location.
			 * @return The LIR locations, possibly with some discarded.
			 */
			[[nodiscard]] std::vector<LIRValue> getLocations(const std::vector<mir::MIRValue>& locs
			) const {
				std::vector<LIRValue> result;
				result.reserve(locs.size());
				for (const auto& loc: locs)
					// Discard information-less locations.
					if (auto lir_loc = getLocation(loc); lir_loc.has_value())
						result.push_back(lir_loc.value());
				return result;
			}

			/**
			 * @brief Lowers flags of the given operation into
			 * LIR instruction flags.
			 *
			 * @note This function will not work correctly when one MIR instruction translates into
			 * multiple LIR instructions, since the `ScopeStart` flags should only be applied to the
			 * first of the LIR instructions and the `ScopeEnd` flags should only be applied to the
			 * last of the LIR instructions.
			 *
			 * @param mir_instruction
			 */
			std::vector<ScopeFlag> lowerFlags(const mir::Instruction& mir_instruction) {
				std::vector<ScopeFlag> result;
				for (const auto& [flag, local]: mir_instruction.flags) {
					// Discard flags for information-less locals.
					if (!local->carriesInformation(ctx)) continue;

					[[maybe_unused]]
					//< temporary for linter
					auto lir_local
						= getLocal(local);

					switch (flag) {
						using enum mir::OperationFlag::Flag;
					case ScopeStart:
						result.push_back({ ScopeFlag::Flag::ScopeStart, lir_local });
						break;
					case ScopeEnd:
						result.push_back({ ScopeFlag::Flag::ScopeEnd, lir_local });
						break;
					default:
						break;
					}
				}
				return result;
			}

			static bool isArgSigned(const mir::MIRValue& location) {
				variant_match(location.getVariant()) {
					variant_case(
						mir::MIRConstant, value
					) {  // @TODO: #899 Remove this visit once CTV is VMValue based and stores it's type.
						match_optional(value.value.get<numeric_value::NumericValue>()) {
							opt_some(numeric) {
								return std::visit(
									[&](auto&& val) {
										using T = std::decay_t<decltype(val)>;
										if constexpr (std::is_signed_v<T>)
											return true;
										else
											return false;
									},
									numeric.getStorage()
								);
							}
							opt_none { return false; }
						}
					}
					variant_case(mir::MIRPlace, place) {
						const auto arg_type = place.type.getType();
						return arg_type.getKind() == tsh::Kind::Integral
						   and tsh::IntegralAbstractType(arg_type).getSignedness()
						           == tsh::IntegralAbstractType::Signedness::Signed;
					}
					variant_default { return false; }
				}
				CORE_UNREACHABLE();
			}

			/**
			 * @brief Lowers instruction from MIR to LIR.
			 * * Fills @p curr_block.
			 * Legal to use only in lowerBlocks
			 * @param curr_block
			 * @return next curr_block
			 */
			MutBlockRef lowerInstruction(
				MutBlockRef curr_block, const mir::Instruction& mir_instruction
			) {
				// curr_block already in order

				CORE_ASSERT(
					not mir::isTerminating(mir_instruction.operation),
					"Terminator in lowerInstruction"
				);
				auto before_instruction_count = curr_block->instructions.size();

				switch (mir_instruction.operation) {
				case mir::Operation::Nop: {
					break;
				}
				case mir::Operation::Assign: {
					CORE_ASSERT(
						mir_instruction.arguments.size() == 1, "Assign should have one argument"
					);
					auto arg     = mir_instruction.arguments[0];
					auto lir_arg = getLocation(arg);
					// The assignment is discarded if it operates on no information.
					if (lir_arg.has_value()) {
						auto output = getOutput(mir_instruction.output);
						curr_block->instructions.emplace_back(
							Operation::Assign,
							output,
							std::vector{ lir_arg.value() },
							mir_instruction.metadata
						);
					}
					break;
				}
				case mir::Operation::ZeroInitialize: {
					auto output = getOutput(mir_instruction.output);

					if (output.has_value()) {
						curr_block->instructions.emplace_back(
							Operation::ZeroInitialize,
							output,
							std::vector<LIRValue>{},
							mir_instruction.metadata
						);
					}
					break;
				}
				case mir::Operation::ListPush:
				case mir::Operation::ListPop: {
					auto output = getOutput(mir_instruction.output);
					auto args   = getLocations(mir_instruction.arguments);

					const auto& list_place = mir_instruction.arguments.at(0).get<mir::MIRPlace>();

					auto dynamic_array_type
						= list_place.type.getType().as<tsh::DynamicArrayAbstractType>();
					auto element_layout = CRef<tsl::TypeLayout>(
						&ctx.query<tsl::QuerySymbolTypeLayout>(dynamic_array_type.getElementType())
							 ->valueOrPanicMsg("layout query failed at LIR stage")
					);

					curr_block->instructions.emplace_back(
						mir2lirOperation(mir_instruction.operation, false),
						output,
						std::move(args),
						InstructionMetadata{},
						ListOperationParameters{ .element_layout = element_layout }
					);
					break;
				}
				case mir::Operation::AddressOf:
				case mir::Operation::ListLen:
				case mir::Operation::BoxAlloc:
				case mir::Operation::IntegerAdd:
				case mir::Operation::IntegerNeg:
				case mir::Operation::IntegerSub:
				case mir::Operation::IntegerMul:
				case mir::Operation::IntegerDiv:
				case mir::Operation::IntegerMod:
				case mir::Operation::IntegerLt:
				case mir::Operation::IntegerGt:
				case mir::Operation::IntegerLteq:
				case mir::Operation::IntegerGteq:
				case mir::Operation::IntegerEq:
				case mir::Operation::IntegerNeq:

				case mir::Operation::FloatAdd:
				case mir::Operation::FloatSub:
				case mir::Operation::FloatMul:
				case mir::Operation::FloatDiv:
				case mir::Operation::FloatNeg:

				case mir::Operation::FloatLt:
				case mir::Operation::FloatGt:
				case mir::Operation::FloatLteq:
				case mir::Operation::FloatGteq:
				case mir::Operation::FloatEq:
				case mir::Operation::FloatNeq:

				case mir::Operation::MetaCreateBox:
				case mir::Operation::MetaCreateRef:
				case mir::Operation::MetaCreateConst:
				case mir::Operation::MetaCreateTuple:
				case mir::Operation::MetaCreateVariant:
				case mir::Operation::MetaEq:
				case mir::Operation::MetaNeq:

				case mir::Operation::BooleanAnd:
				case mir::Operation::BooleanOr:
				case mir::Operation::BooleanNot: {
					// this is a generic case, that will be used for most instructions
					// it currently assumes the output is present, but it can be changed
					auto output = getOutput(mir_instruction.output);

					auto args = getLocations(mir_instruction.arguments);

					// It is assumed that all arguments of a built-in function are of the same exact
					// type, and thus also have the same sign (if that matters). Any conversions
					// should have been handled by HELIoS.
					// @TODO: Refine this check.
					const auto use_signed_version = isArgSigned(mir_instruction.arguments.at(0));
					curr_block->instructions.emplace_back(
						mir2lirOperation(mir_instruction.operation, use_signed_version),
						output,
						std::move(args),
						mir_instruction.metadata
					);
					break;
				}
				case mir::Operation::DestructIf: {
					const auto& to_destruct = mir_instruction.arguments.at(0).get<mir::MIRPlace>();
					const auto& type        = to_destruct.type;

					// @TODO: #2825 The whole DestructIf implementation is a stub. Implement it once
					// we know how to call destructors.

					if (type.getRefKind() == tsh::ReferenceKind::Direct
					    && type.getType().getKind() == tsh::Kind::DynamicArray) {
						auto lir_place = getLocation(to_destruct);

						if (lir_place.has_value()) {
							curr_block->instructions.emplace_back(
								Operation::ListFree,
								base::Optional<LIRPlace>{},
								std::vector{ lir_place.value() },
								InstructionMetadata{}
							);
							break;
						}
					}

					// @TODO: #1894 This is a stub just to test the overall box free'ing logic.
					// Currently this approach generates a free on every DestructIf if it operates
					// on a box type (even if the box was moved). This will cause double free's if
					// the box was moved around between other box variables. In the future we should
					// insert a proper destructor call here before the FreeBox.
					if (type.getRefKind() == tsh::ReferenceKind::Box) {
						auto lir_place = getLocation(to_destruct);

						// FreeBox is discarded if it operates on no information (ex. Unit).
						if (lir_place.has_value()) {
							curr_block->instructions.emplace_back(
								Operation::BoxFree,
								base::Optional<LIRPlace>{},
								std::vector{ lir_place.value() },
								mir_instruction.metadata
							);
							break;
						}
					}

					CORE_DEV_LOG(
						Compiler,
						"DestructIf not implemented for types with non-trivial "
						"destructors, skipping",
						"\n"
					);

					break;
				}
				case mir::Operation::Call: {
					auto output = getOutput(mir_instruction.output);
					auto args   = getLocations(mir_instruction.arguments);
					curr_block->instructions.emplace_back(
						Operation::Call, output, std::move(args), mir_instruction.metadata
					);
					break;
				}
				case mir::Operation::Cast: {
					auto cast_parameters
						= std::get_if<mir::CastParameters>(&mir_instruction.extra_params);
					if (not cast_parameters) CORE_PANIC("Cast instruction without CastParameters");

					auto args   = getLocations(mir_instruction.arguments);
					auto output = getOutput(mir_instruction.output);
					curr_block->instructions.emplace_back(
						Operation::Cast,
						output,
						std::move(args),
						mir_instruction.metadata,
						CastParameters{
							.source_type = cast_parameters->source_type,
							.target_type = cast_parameters->target_type,
							.source_layout
							= &ctx.query<tsl::QuerySymbolTypeLayout>(cast_parameters->source_type)
					               ->valueOrPanicMsg("layout query failed at LIR stage"),
							.target_layout
							= &ctx.query<tsl::QuerySymbolTypeLayout>(cast_parameters->target_type)
					               ->valueOrPanicMsg("layout query failed at LIR stage"),
						}
					);
					break;
				}
				default:
					throw base::NotYetImplemented(base::strConcat(
						"instruction ",
						base::enumToStr(mir_instruction.operation),
						" in LowerToLIRFunction"
					));
				}
				usize after_instruction_count = curr_block->instructions.size();
				auto  flags                   = lowerFlags(mir_instruction);
				if (!flags.empty()) {
					usize instructions_added = after_instruction_count - before_instruction_count;
					// If this fails, then it's no problem, we just have to adjust the code.
					// The `ScopeStart` flags should be added to the first of the LIR instructions
					// and the `ScopeEnd` flags should be added to the last of the LIR instructions.
					// Now they are added to both in one place.
					CORE_ASSERT(
						instructions_added <= 1,
						base::strConcat(
							"lowerFlags expected the increase to be less equal to 1, but it was ",
							after_instruction_count - before_instruction_count
						)
					);
					if (instructions_added == 0) {
						curr_block->instructions.emplace_back(
							Operation::Nop,
							base::Optional<LIRPlace>{},
							std::vector<LIRValue>{},
							mir_instruction.metadata
						);
					}
					curr_block->instructions.back().scope_flags.insert(
						curr_block->instructions.back().scope_flags.end(), flags.begin(), flags.end()
					);
				}
				return curr_block;
			}

			/**
			 * @brief Lowers terminator from MIR to LIR.
			 * Fills @p curr_block.
			 * Legal to use only in lowerBlocks
			 * @param curr_block
			 * @return next curr_block
			 */
			void lowerTerminator(MutBlockRef curr_block, const mir::Instruction& mir_terminator) {
				// @TODO
				// curr_block already in order
				CORE_ASSERT(
					mir::isTerminating(mir_terminator.operation), "non-Terminator in lowerTerminator"
				);

				CORE_ASSERT(mir_terminator.output.empty(), "terminator should not return");
				switch (mir_terminator.operation) {
				case mir::Operation::ReturnValue: {
					// The returned value may have been discarded due to being information-less.
					if (auto args = getLocations(mir_terminator.arguments); args.size() == 0)
						curr_block->terminator
							= Instruction{ Operation::ReturnVoid, {}, {}, mir_terminator.metadata };
					else
						curr_block->terminator = Instruction{
							Operation::ReturnValue, {}, std::move(args), mir_terminator.metadata
						};
					break;
				}
				case mir::Operation::ReturnVoid:
				case mir::Operation::Jump:
				case mir::Operation::Branch: {
					auto args = getLocations(mir_terminator.arguments);
					curr_block->terminator
						= Instruction{ mir2lirOperation(mir_terminator.operation, false),
						               {},
						               std::move(args),
						               mir_terminator.metadata };
					break;
				}
				case mir::Operation::FunctionEnd: {
					CORE_PANIC("FunctionEnd is illegal outside of MIRLowering phase");
					break;
				}
				// @TODO: add more cases
				default:
					throw base::NotYetImplemented("terminator in LowerToLIRFunction");
				}

				curr_block->terminator.scope_flags = lowerFlags(mir_terminator);
			}

			/**
			 * @brief Return function composed of generated data.
			 * Should be used once, at the end of LIR function creation.
			 * @return Function
			 */
			Function get() && {
				auto return_type = mirReturnType2LirLayout(ctx, key.function->return_type);
				std::vector<CRef<tsl::TypeLayout>> parameter_types;
				parameter_types.reserve(key.function->parameter_types.size());
				for (const auto& param: key.function->parameter_types)
					// Discard information-less parameters from LIR function parameter lists.
					if (param.getType().carriesInformation(ctx))
						parameter_types.emplace_back(
							&ctx.query<tsl::QuerySymbolTypeLayout>(param)->valueOrPanicMsg(
								"layout query failed at LIR stage"
							)
						);


				auto abi = [&]() -> helios::SymbolABI {
					variant_match(key.function->helios_id) {
						variant_case(mir::FunctionSymID, name) {
							return ctx.query<helios::QuerySymbolABI>(name.id)->valueOrPanicMsg(
								"Handling errors in MIR is not supported yet"
							);
						}
						variant_case(mir::GlobalVariableCTOR, name) { return helios::DefaultAbi{}; }
					}
					CORE_UNREACHABLE();
				}();

				auto [link_once, ignore_on_dvm, ignore_on_llvm] = [&]() {
					variant_match(key.function->helios_id) {
						variant_case(mir::FunctionSymID, sym) {
							bool link_once_val = helios::shouldLinkOnce(sym.id);
							bool ignore_on_dvm_val
								= helios::hasAttribute<helios::attributes::NativeOnlyImpl>(sym.id);
							bool ignore_on_llvm_val
								= helios::hasAttribute<helios::attributes::DVMOnlyImpl>(sym.id);
							return std::make_tuple(
								link_once_val, ignore_on_dvm_val, ignore_on_llvm_val
							);
						}
						variant_case(mir::GlobalVariableCTOR, name) {
							return std::make_tuple(false, false, false);
						}
					}
					CORE_UNREACHABLE();
				}();

				auto mangled_name = [&]() {
					variant_match(key.function->helios_id) {
						variant_case(mir::FunctionSymID, name) {
							return helios::mangler::getSimpleMangledName(ctx, name.id);
						}
						variant_case(mir::GlobalVariableCTOR, name) {
							return helios::mangler::getSpecialMangledName<
								helios::mangler::ManglingSymbolKind::GlobalVariableConstructor>(
								ctx, name.global_var_id
							);
						}
					}
					CORE_UNREACHABLE();
				}();

				FunctionMetadata metadata;
				auto&            mir_func = key.function;
				if (auto func_id = std::get_if<mir::FunctionSymID>(&mir_func->helios_id)) {
					if (auto pst_elem = helios::maybeSymbolPst(func_id->id)) {
						metadata.position         = (*pst_elem).unlock(ctx)->getStablePosition();
						metadata.source_code_name = helios::name(func_id->id);
					}
				}

				return Function{ .mangled_name       = mangled_name,
					             .abi                = abi,
					             .link_once          = link_once,
					             .ignore_on_dvm      = ignore_on_dvm,
					             .ignore_on_llvm     = ignore_on_llvm,
					             .parameter_layouts  = std::move(parameter_types),
					             .return_type_layout = return_type,
					             .blocks             = std::move(blocks),
					             .local_list         = std::move(locals),
					             .block_order        = std::move(block_order),
					             .metadata           = metadata };
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			// LIRFunction is build-inplace here, and for this reason
			// achieves legal state only at the end.
			// for this reason additional assertions should be put inplace, to
			// ensure that the state is legal.

			// what we do here:
			// * map mir locals to lir locals
			// * map mir block numbers to lir blocks
			// * go thru all blocks
			// * generate lir-blocks from each mir-block, by:
			//   * going thru all instructions
			//   * generating lir-instructions and lir-blocks from each mir-instruction

			MIR2LIR mir2lir{ ctx, key };

			// call order matters:
			mir2lir.makeLocals();
			mir2lir.makeInitialBlocks();
			mir2lir.lowerBlocks();

			auto fun = std::move(mir2lir).get();

			// @opt: remove it in optimized, release builds
			CORE_ASSERT(fun.validateBlockOrder().isOk(), "Invalid block order");
			CORE_ASSERT(fun.validateParameters().isOk(), "Invalid parameters");

			return fun;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LowerToLIRFunction);

	Function createFunctionInvoker(
		query::Context&                    ctx,
		const std::vector<CRef<Function>>& functions,
		const base::StrID&                 mangled_name
	) {
		auto function_type = ctx.query<tsh::QueryFunctionType>({
			.parameter_types={},
			.result_type=tsh::SymbolType{
				tsh::getUnitType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			},
		});

		auto return_type = CRef<tsl::TypeLayout>(
			&ctx.query<tsl::QuerySymbolTypeLayout>(function_type.getResultType())
				 ->valueOrPanicMsg("layout query failed at LIR stage")
		);

		Block entry_block;
		entry_block.terminator = Instruction{ Operation::ReturnVoid, {}, {}, {} };

		for (const auto& function: functions) {
			entry_block.instructions.push_back(Instruction{
				Operation::Call, {}, { LIRValue{ FunctionLiteral::fromFunction(*function) } }, {} });
		}

		base::StableVector<Block> blocks;
		blocks.emplaceBack(std::move(entry_block));

		BlockRef entry_block_ref = blocks.last();

		return Function{ .mangled_name       = mangled_name,
			             .abi                = helios::DefaultAbi{},
			             .link_once          = false,
			             .ignore_on_dvm      = false,
			             .ignore_on_llvm     = false,
			             .parameter_layouts  = {},
			             .return_type_layout = return_type,
			             .blocks             = std::move(blocks),
			             .local_list         = {},
			             .block_order        = { entry_block_ref },
			             .metadata           = { .position = {}, .source_code_name = {} } };
	}

}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "template_deduction.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>

namespace compiler::helios::templates {
	base::Bit256 TemplateDeductionKey::queryUnstablePerfectHash() const {
		hashing::SHA256 hasher;
		hashing::addToHash(hasher, template_sym_id.queryUnstablePerfectHash());
		hashing::addToHash(hasher, positional_argument_types.size());
		for (const auto& type: positional_argument_types)
			hashing::addToHash(hasher, type.queryUnstablePerfectHash());
		hashing::addToHash(hasher, named_argument_types.size());
		for (const auto& [name, type]: named_argument_types) {
			hashing::addToHash(hasher, name);
			hashing::addToHash(hasher, type.queryUnstablePerfectHash());
		}
		return hasher.finalize();
	}

	namespace {
		/**
		 * @brief A parameter of the function template baked with placeholders.
		 */
		struct ParameterPattern final {
			base::StrID      name;
			tsh::ValueType<> type;
		};

		/**
		 * @brief Whether the template parameter is a `typeoperator`.
		 */
		bool isTypeOperatorParameter(
			query::Context& ctx, const TemplateDeclarationSignature::Parameter& parameter
		) {
			return parameter.type.getType() == tsh::getTypeOperatorType(ctx);
		}

		/**
		 * @brief The declared parameters of a function, a function declaration or a method.
		 */
		pst::AccessLocked<pst::ParamList> getFunctionLikeParams(
			const pst::Access<pst::LangElement>& function_like
		) {
			switch (function_like->getElementKind()) {
			case pst::ElementKind::Fun:
				return function_like.dynamicCast<pst::Fun>().value()->getParams();
			case pst::ElementKind::FunDecl:
				return function_like.dynamicCast<pst::FunDecl>().value()->getParams();
			case pst::ElementKind::ClassMethod:
				return function_like.dynamicCast<pst::Method>().value()->getParams();
			default:
				CORE_PANIC("Deduction on a template of a non-function");
			}
		}

		/**
		 * @brief Bakes the function template with a placeholder for each of its parameters and
		 * returns the parameters of the baked function.
		 */
		std::vector<ParameterPattern> getParameterPatterns(
			query::Context& ctx, SymID template_sym_id, const TemplateDeclarationSignature& signature
		) {
			TemplateBakeKey key{ .template_sym_id = template_sym_id, .template_arguments = {} };
			for (const auto& [index, parameter]: std::views::enumerate(signature.parameters)) {
				const auto placeholder = ctx.query<tsh::QueryTemplateDeductionPlaceholderType>({
					.template_symbol = template_sym_id,
					.parameter_index = static_cast<usize>(index),
				});
				if (isTypeOperatorParameter(ctx, parameter))
					key.template_arguments.emplace_back(ctv::CompileTimeValue::TypeOperator{
						.op = placeholder });
				else
					key.template_arguments.emplace_back(
						tsh::ValueType<>::withDefaultsMut(placeholder)
					);
			}

			const SymID baked  = ctx.query<QueryBakeTemplateSymID>(key).valueOrThrow();
			const auto  params = getFunctionLikeParams(stmt(ctx, baked).value()).unlock(ctx);

			std::vector<ParameterPattern> patterns;
			for (auto param_locked: *params) {
				const auto param        = param_locked.unlock(ctx);
				const auto param_symbol = ctx.query<QuerySymbolOfSTMT>({ param }).valueOrThrow();
				patterns.push_back({
					.name = name(param_symbol),
					.type = ctx.query<QueryTypeOfSymbol>({ param_symbol })->valueOrThrow(),
				});
			}
			return patterns;
		}

		/**
		 * @brief The template instance data of @p type, if it is a class baked from a class
		 * template.
		 */
		base::Optional<CRef<TemplateInstanceSemantics>> classTemplateInstanceOf(
			const tsh::AbstractType& type
		) {
			if (type.getKind() != tsh::Kind::Class) return {};
			return getSymRef(tsh::ClassAbstractType(type).getSymbol())
			    ->getDataOpt<TemplateInstanceSemantics>();
		}

		/**
		 * @brief The value of the constant holding a template argument of a template instance.
		 */
		const ctv::CompileTimeValue& templateArgumentValue(SymID argument_symbol) {
			return getSymRef(argument_symbol)->getData<defgen::GeneratedConstant>()->value;
		}

		bool containsPlaceholder(const tsh::AbstractType& type);

		/**
		 * @brief Whether a placeholder occurs anywhere in the template argument @p value.
		 */
		bool containsPlaceholder(const ctv::CompileTimeValue& value) {
			variant_match(value.getStorage()) {
				variant_case(tsh::ValueType<>, type) { return containsPlaceholder(type.getType()); }
				variant_case(ctv::CompileTimeValue::TypeOperator, type_operator) {
					return std::holds_alternative<tsh::TemplateDeductionPlaceholderAbstractType>(
						type_operator.op
					);
				}
				variant_case(ctv::CompileTimeValue::TypeTuple, tuple) {
					return std::ranges::any_of(tuple.getElements(), [](const auto& element) {
						return containsPlaceholder(
							ctv::CompileTimeValue::TypeTuple::elementToCtv(element)
						);
					});
				}
			}
			return false;
		}

		/**
		 * @brief Whether a placeholder occurs anywhere in @p type.
		 */
		bool containsPlaceholder(const tsh::AbstractType& type) {
			const auto any_of = [](const std::vector<tsh::ValueType<>>& types) {
				return std::ranges::any_of(types, [](const tsh::ValueType<>& inner) {
					return containsPlaceholder(inner.getType());
				});
			};

			switch (type.getKind()) {
			case tsh::Kind::TemplateDeductionPlaceholder:
				return true;
			case tsh::Kind::Pointer:
				return containsPlaceholder(tsh::PointerAbstractType(type).getUnderlyingType());
			case tsh::Kind::ManyPointer:
				return containsPlaceholder(tsh::ManyPointerAbstractType(type).getUnderlyingType());
			case tsh::Kind::CPointer:
				return containsPlaceholder(tsh::CPointerAbstractType(type).getUnderlyingType());
			case tsh::Kind::Slice:
				return containsPlaceholder(tsh::SliceAbstractType(type).getElementType().getType());
			case tsh::Kind::StaticArray:
				return containsPlaceholder(
					tsh::StaticArrayAbstractType(type).getElementType().getType()
				);
			case tsh::Kind::Tuple:
				return any_of(tsh::TupleAbstractType(type).getComponents());
			case tsh::Kind::Variant:
				return any_of(tsh::VariantAbstractType(type).getUnderlyingTypes());
			case tsh::Kind::Function: {
				const tsh::FunctionAbstractType function(type);
				return any_of(function.getParameterTypes())
				    or containsPlaceholder(function.getResultType().getType());
			}
			case tsh::Kind::Class: {
				const auto instance = classTemplateInstanceOf(type);
				return instance.has_value()
				   and std::ranges::any_of(
						   instance.value()->template_arguments_symbols,
						   [](SymID argument) {
							   return containsPlaceholder(templateArgumentValue(argument));
						   }
				   );
			}
			default:
				return false;
			}
		}

		/**
		 * @brief Matches parameter types with placeholders against argument types and records the
		 * first value found for each template parameter.
		 */
		class TemplateArgumentDeducer final {
			using TypeOperator = ctv::CompileTimeValue::TypeOperator;

			query::Context&                                    ctx;
			std::vector<base::Optional<ctv::CompileTimeValue>> deduced;

			void bind(usize parameter_index, ctv::CompileTimeValue value) {
				if (deduced.at(parameter_index).has_value()) return;
				deduced.at(parameter_index) = std::move(value);
			}

			static usize placeholderIndex(const tsh::AbstractType& type) {
				return tsh::TemplateDeductionPlaceholderAbstractType(type).getParameterIndex();
			}

			static base::Optional<tsh::ValueType<>> placeholderOperand(const tsh::AbstractType& type
			) {
				if (type.getKind() != tsh::Kind::TemplateDeductionPlaceholder) return {};
				return tsh::TemplateDeductionPlaceholderAbstractType(type).getOperand();
			}

			static TypeOperator::Kind typeOperatorOf(tsh::ReferenceKind ref_kind) {
				switch (ref_kind) {
				case tsh::ReferenceKind::Direct:
					return TypeOperator::Kind::Identity;
				case tsh::ReferenceKind::Ref:
					return TypeOperator::Kind::Ref;
				case tsh::ReferenceKind::Box:
					return TypeOperator::Kind::Box;
				}
				CORE_UNREACHABLE();
			}

			/**
			 * @brief Matches a `typeoperator` placeholder applied to @p operand, like `refkind T`.
			 * The reference kind of the argument determines the operator, and the operand is
			 * matched with the direct argument.
			 * @return Whether @p param is such a placeholder.
			 */
			bool deduceTypeOperator(
				const tsh::ValueType<>& param, const tsh::ValueType<>& arg, bool nested
			) {
				const auto operand = placeholderOperand(param.getType());
				if (not operand.has_value()) return false;
				if (param.getRefKind() != tsh::ReferenceKind::Direct) return true;

				bind(
					placeholderIndex(param.getType()),
					TypeOperator{ .op = typeOperatorOf(arg.getRefKind()) }
				);
				if (nested)
					deduceNested(operand.value(), arg.toDirect());
				else
					deduce(operand.value(), arg.toDirect());
				return true;
			}

			/**
			 * @brief Matches a type nested in another one, where the reference kind and the
			 * mutability are part of the matched value.
			 */
			void deduceNested(const tsh::ValueType<>& param, const tsh::ValueType<>& arg) {
				if (deduceTypeOperator(param, arg, true)) return;

				if (param.getType().getKind() != tsh::Kind::TemplateDeductionPlaceholder) {
					deduceStructure(param.getType(), arg);
					return;
				}

				if (param.getRefKind() == tsh::ReferenceKind::Direct)
					bind(placeholderIndex(param.getType()), arg);
				else if (param.getRefKind() == arg.getRefKind())
					bind(placeholderIndex(param.getType()), arg.toDirect());
			}

			void deduceEach(
				const std::vector<tsh::ValueType<>>& params,
				const std::vector<tsh::ValueType<>>& args
			) {
				if (params.size() != args.size()) return;
				for (const auto& [param, arg]: std::views::zip(params, args))
					deduceNested(param, arg);
			}

			/**
			 * @brief Matches the alternatives of a variant. The alternatives without placeholders
			 * are removed from the argument, and the single alternative with placeholders is
			 * matched with what is left: one alternative, or a variant of all of them when the
			 * alternative is a bare placeholder. An argument which is not a variant is matched with
			 * the alternative with placeholders, unless it is one of the other alternatives.
			 */
			void deduceVariant(const tsh::VariantAbstractType& param, const tsh::ValueType<>& arg) {
				std::vector<tsh::ValueType<>>  patterns;
				std::vector<tsh::AbstractType> concrete;
				for (const auto& alternative: param.getUnderlyingTypes())
					if (containsPlaceholder(alternative.getType()))
						patterns.push_back(alternative);
					else
						concrete.push_back(alternative.getType());
				if (patterns.size() != 1) return;
				const auto& pattern = patterns.front();

				if (arg.getType().getKind() != tsh::Kind::Variant) {
					if (std::ranges::contains(concrete, arg.getType())) return;
					if (placeholderOperand(pattern.getType()).has_value())
						deduceNested(pattern, arg);
					else
						deduceNested(pattern, tsh::ValueType<>::withDefaultsMut(arg.getType()));
					return;
				}

				std::vector<tsh::ValueType<>> rest;
				for (const auto& alternative:
				     tsh::VariantAbstractType(arg.getType()).getUnderlyingTypes())
					if (not std::ranges::contains(concrete, alternative.getType()))
						rest.push_back(alternative);

				if (rest.size() == 1) {
					deduceNested(pattern, rest.front());
					return;
				}

				if (rest.size() > 1
				    and pattern.getType().getKind() == tsh::Kind::TemplateDeductionPlaceholder
				    and pattern.getRefKind() == tsh::ReferenceKind::Direct) {
					const auto rest_variant = ctx.query<tsh::QueryVariantType>({ rest });
					bind(
						placeholderIndex(pattern.getType()),
						tsh::ValueType<>::withDefaultsMut(rest_variant)
					);
				}
			}

			/**
			 * @brief Matches a template argument of a class instance with the corresponding
			 * template argument of another instance of the same class template.
			 */
			void deduceTemplateArgument(
				const ctv::CompileTimeValue& param, const ctv::CompileTimeValue& arg
			) {
				if (not containsPlaceholder(param)) return;

				variant_match(param.getStorage()) {
					variant_case(tsh::ValueType<>, param_type) {
						const auto arg_type = arg.get<tsh::ValueType<>>();
						if (arg_type.has_value()) deduceNested(param_type, arg_type.value());
					}
					variant_case(TypeOperator, param_operator) {
						const auto arg_operator = arg.get<TypeOperator>();
						if (arg_operator.has_value()
						    and std::holds_alternative<TypeOperator::Kind>(arg_operator->op))
							bind(
								std::get<tsh::TemplateDeductionPlaceholderAbstractType>(
									param_operator.op
								)
									.getParameterIndex(),
								arg_operator.value()
							);
					}
					variant_case(ctv::CompileTimeValue::TypeTuple, param_tuple) {
						const auto arg_tuple = arg.get<ctv::CompileTimeValue::TypeTuple>();
						if (not arg_tuple.has_value()
						    or param_tuple.getElements().size() != arg_tuple->getElements().size())
							return;
						for (const auto& [param_element, arg_element]:
						     std::views::zip(param_tuple.getElements(), arg_tuple->getElements()))
							deduceTemplateArgument(
								ctv::CompileTimeValue::TypeTuple::elementToCtv(param_element),
								ctv::CompileTimeValue::TypeTuple::elementToCtv(arg_element)
							);
					}
				}
			}

			/**
			 * @brief Matches the template arguments of two instances of the same class template.
			 */
			void deduceClassTemplateInstance(
				const tsh::AbstractType& param, const tsh::AbstractType& arg
			) {
				const auto param_instance = classTemplateInstanceOf(param);
				const auto arg_instance   = classTemplateInstanceOf(arg);
				if (not param_instance.has_value() or not arg_instance.has_value()
				    or param_instance.value()->template_symbol
				           != arg_instance.value()->template_symbol)
					return;

				for (const auto& [param_argument, arg_argument]: std::views::zip(
						 param_instance.value()->template_arguments_symbols,
						 arg_instance.value()->template_arguments_symbols
					 ))
					deduceTemplateArgument(
						templateArgumentValue(param_argument), templateArgumentValue(arg_argument)
					);
			}

			/**
			 * @brief Peels off the same kind of type from the parameter and the argument, and
			 * matches their components.
			 */
			void deduceStructure(const tsh::AbstractType& param, const tsh::ValueType<>& arg_symbol) {
				if (not containsPlaceholder(param)) return;

				if (param.getKind() == tsh::Kind::Variant) {
					deduceVariant(tsh::VariantAbstractType(param), arg_symbol);
					return;
				}

				const tsh::AbstractType arg = arg_symbol.getType();

				if (param.getKind() != arg.getKind()) return;

				switch (param.getKind()) {
				case tsh::Kind::Pointer:
					deduceNested(
						tsh::PointerAbstractType(param).getPointee(),
						tsh::PointerAbstractType(arg).getPointee()
					);
					return;
				case tsh::Kind::ManyPointer:
					deduceNested(
						tsh::ManyPointerAbstractType(param).getPointee(),
						tsh::ManyPointerAbstractType(arg).getPointee()
					);
					return;
				case tsh::Kind::CPointer:
					deduceNested(
						tsh::CPointerAbstractType(param).getPointee(),
						tsh::CPointerAbstractType(arg).getPointee()
					);
					return;
				case tsh::Kind::Slice:
					deduceNested(
						tsh::SliceAbstractType(param).getElementType(),
						tsh::SliceAbstractType(arg).getElementType()
					);
					return;
				case tsh::Kind::StaticArray:
					deduceNested(
						tsh::StaticArrayAbstractType(param).getElementType(),
						tsh::StaticArrayAbstractType(arg).getElementType()
					);
					return;
				case tsh::Kind::Tuple:
					deduceEach(
						tsh::TupleAbstractType(param).getComponents(),
						tsh::TupleAbstractType(arg).getComponents()
					);
					return;
				case tsh::Kind::Function: {
					const tsh::FunctionAbstractType param_function(param);
					const tsh::FunctionAbstractType arg_function(arg);
					deduceEach(param_function.getParameterTypes(), arg_function.getParameterTypes());
					deduceNested(param_function.getResultType(), arg_function.getResultType());
					return;
				}
				case tsh::Kind::Class:
					deduceClassTemplateInstance(param, arg);
					return;
				default:
					return;
				}
			}

		public:
			TemplateArgumentDeducer(query::Context& ctx, usize parameter_count):
				  ctx(ctx),
				  deduced(parameter_count) {}

			/**
			 * @brief Matches the type of a function parameter with the type of its argument. The
			 * reference kind and the mutability of the argument only say how it is passed, so a
			 * placeholder at the top is bound to the bare type of the argument.
			 */
			void deduce(const tsh::ValueType<>& param, const tsh::ValueType<>& arg) {
				if (deduceTypeOperator(param, arg, false)) return;

				if (param.getType().getKind() == tsh::Kind::TemplateDeductionPlaceholder) {
					bind(
						placeholderIndex(param.getType()),
						tsh::ValueType<>::withDefaultsMut(arg.getType())
					);
					return;
				}
				deduceStructure(param.getType(), arg);
			}

			[[nodiscard]] bool isComplete() const {
				return std::ranges::all_of(deduced, [](const auto& value) {
					return value.has_value();
				});
			}

			[[nodiscard]] base::Optional<usize> firstUndeduced() const {
				for (usize i = 0; i < deduced.size(); i++)
					if (not deduced.at(i).has_value()) return i;
				return {};
			}

			[[nodiscard]] std::vector<ctv::CompileTimeValue> getArguments() const {
				std::vector<ctv::CompileTimeValue> arguments;
				arguments.reserve(deduced.size());
				for (const auto& value: deduced) arguments.push_back(value.value());
				return arguments;
			}
		};
	}

	struct IMPLEMENT_QUERY(QueryTemplateArgumentDeduction, query::QResult<TemplateDeductionResult>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			CORE_ASSERT(
				isFunctionTemplate(ctx, key.template_sym_id),
				"Template argument deduction on a template of a non-function"
			);

			const auto signature
				= getTemplateDeclarationSignature(ctx, key.template_sym_id).valueOrThrow();

			for (const auto& [index, parameter]: std::views::enumerate(signature.parameters)) {
				if (parameter.type.getType().getKind() != tsh::Kind::Meta
				    and not isTypeOperatorParameter(ctx, parameter)) {
					return TemplateDeductionResult{ TemplateDeductionFailure{
						.reason          = TemplateDeductionFailure::Reason::NonTypeParameter,
						.parameter_index = static_cast<usize>(index),
						.parameter_name  = parameter.name,
					} };
				}
			}

			const auto patterns = getParameterPatterns(ctx, key.template_sym_id, signature);

			TemplateArgumentDeducer deducer(ctx, signature.parameters.size());

			for (const auto& [pattern, arg]:
			     std::views::zip(patterns, key.positional_argument_types)) {
				if (deducer.isComplete()) break;
				deducer.deduce(pattern.type, arg);
			}

			for (const auto& [arg_name, arg]: key.named_argument_types) {
				if (deducer.isComplete()) break;
				const auto pattern = std::ranges::find(patterns, arg_name, &ParameterPattern::name);
				if (pattern != patterns.end()) deducer.deduce(pattern->type, arg);
			}

			if (const auto undeduced = deducer.firstUndeduced(); undeduced.has_value()) {
				return TemplateDeductionResult{ TemplateDeductionFailure{
					.reason          = TemplateDeductionFailure::Reason::Undeduced,
					.parameter_index = undeduced.value(),
					.parameter_name  = signature.parameters.at(undeduced.value()).name,
				} };
			}

			return TemplateDeductionResult{ deducer.getArguments() };
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTemplateArgumentDeduction);
}

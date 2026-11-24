// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "template_deduction.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

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
			base::StrID       name;
			tsh::SymbolType<> type;
		};

		/**
		 * @brief Bakes the function template with a placeholder for each of its type parameters
		 * and returns the parameters of the baked function.
		 */
		std::vector<ParameterPattern> getParameterPatterns(
			query::Context& ctx, SymID template_sym_id, usize template_parameter_count
		) {
			TemplateBakeKey key{ .template_sym_id = template_sym_id, .template_arguments = {} };
			for (usize i = 0; i < template_parameter_count; i++) {
				const auto placeholder = ctx.query<tsh::QueryTemplateDeductionPlaceholderType>({
					.template_symbol = template_sym_id,
					.parameter_index = i,
				});
				key.template_arguments.emplace_back(tsh::SymbolType<>::withDefaultsMut(placeholder));
			}

			const SymID baked = ctx.query<QueryBakeTemplateSymID>(key).valueOrThrow();
			CORE_ASSERT(
				kind(baked) == SymbolKind::Function, "Deduction on a template of a non-function"
			);
			const auto fun = stmt(ctx, baked).value().dynamicCast<pst::Fun>().value();

			std::vector<ParameterPattern> patterns;
			for (auto param_locked: *fun->getParams().unlock(ctx)) {
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
		 * @brief Whether a placeholder occurs anywhere in @p type.
		 */
		bool containsPlaceholder(const tsh::AbstractType& type) {
			const auto any_of = [](const std::vector<tsh::SymbolType<>>& types) {
				return std::ranges::any_of(types, [](const tsh::SymbolType<>& inner) {
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
			default:
				return false;
			}
		}

		/**
		 * @brief Matches parameter types with placeholders against argument types and records the
		 * first value found for each template parameter.
		 */
		class TemplateArgumentDeducer final {
			query::Context&                                ctx;
			std::vector<base::Optional<tsh::SymbolType<>>> deduced;

			void bind(usize parameter_index, const tsh::SymbolType<>& value) {
				if (deduced.at(parameter_index).has_value()) return;
				deduced.at(parameter_index) = value;
			}

			static usize placeholderIndex(const tsh::AbstractType& type) {
				return tsh::TemplateDeductionPlaceholderAbstractType(type).getParameterIndex();
			}

			/**
			 * @brief Matches a type nested in another one, where the reference kind and the
			 * mutability are part of the matched value.
			 */
			void deduceNested(const tsh::SymbolType<>& param, const tsh::SymbolType<>& arg) {
				if (param.getType().getKind() != tsh::Kind::TemplateDeductionPlaceholder) {
					deduceStructure(param.getType(), arg.getType());
					return;
				}

				if (param.getRefKind() == tsh::ReferenceKind::Direct)
					bind(placeholderIndex(param.getType()), arg);
				else if (param.getRefKind() == arg.getRefKind())
					bind(placeholderIndex(param.getType()), arg.toDirect());
			}

			void deduceEach(
				const std::vector<tsh::SymbolType<>>& params,
				const std::vector<tsh::SymbolType<>>& args
			) {
				if (params.size() != args.size()) return;
				for (const auto& [param, arg]: std::views::zip(params, args))
					deduceNested(param, arg);
			}

			/**
			 * @brief Matches the alternatives of a variant. The alternatives without placeholders
			 * are removed from the argument, and the single alternative with placeholders is
			 * matched with what is left: one alternative, or a variant of all of them when the
			 * alternative is a bare placeholder.
			 */
			void deduceVariant(const tsh::VariantAbstractType& param, const tsh::AbstractType& arg) {
				std::vector<tsh::SymbolType<>> patterns;
				std::vector<tsh::AbstractType> concrete;
				for (const auto& alternative: param.getUnderlyingTypes())
					if (containsPlaceholder(alternative.getType()))
						patterns.push_back(alternative);
					else
						concrete.push_back(alternative.getType());
				if (patterns.size() != 1) return;
				const auto& pattern = patterns.front();

				if (arg.getKind() != tsh::Kind::Variant) {
					deduceNested(pattern, tsh::SymbolType<>::withDefaultsMut(arg));
					return;
				}

				std::vector<tsh::SymbolType<>> rest;
				for (const auto& alternative: tsh::VariantAbstractType(arg).getUnderlyingTypes())
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
						tsh::SymbolType<>::withDefaultsMut(rest_variant)
					);
				}
			}

			/**
			 * @brief Peels off the same kind of type from the parameter and the argument, and
			 * matches their components.
			 */
			void deduceStructure(const tsh::AbstractType& param, const tsh::AbstractType& arg) {
				if (not containsPlaceholder(param)) return;

				if (param.getKind() == tsh::Kind::Variant) {
					deduceVariant(tsh::VariantAbstractType(param), arg);
					return;
				}

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
			void deduce(const tsh::SymbolType<>& param, const tsh::SymbolType<>& arg) {
				if (param.getType().getKind() == tsh::Kind::TemplateDeductionPlaceholder) {
					bind(
						placeholderIndex(param.getType()),
						tsh::SymbolType<>::withDefaultsMut(arg.getType())
					);
					return;
				}
				deduceStructure(param.getType(), arg.getType());
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
				for (const auto& value: deduced) arguments.emplace_back(value.value());
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
				if (parameter.type.getType().getKind() != tsh::Kind::Meta) {
					return TemplateDeductionResult{ TemplateDeductionFailure{
						.reason          = TemplateDeductionFailure::Reason::NonTypeParameter,
						.parameter_index = static_cast<usize>(index),
						.parameter_name  = parameter.name,
					} };
				}
			}

			const auto patterns
				= getParameterPatterns(ctx, key.template_sym_id, signature.parameters.size());

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

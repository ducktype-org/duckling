// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/elements/hierarchy/declarations/template_stmt.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/except/exceptions.hpp>
#include <base/misc/anycast.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>

#include <ranges>

namespace compiler::tsh {
	namespace {
		enum class TypeNamePrecedence {
			Variant,
			Prefix,
			Postfix,
			Atomic,
		};

		struct SourceTypeName final {
			std::string        text;
			TypeNamePrecedence precedence;

			[[nodiscard]] std::string nestedIn(const TypeNamePrecedence parent_precedence) const {
				return precedence < parent_precedence ? "(" + text + ")" : text;
			}
		};

		SourceTypeName renderSymbolTypeName(query::Context& ctx, SymbolType<> type);

		std::string templateArgumentsSuffix(
			query::Context& ctx, const helios::templates::TemplateBakePSTLinkedData& bake_data
		) {
			const auto postponed = bake_data.postponed_data->load(std::memory_order_acquire);
			CORE_ASSERT(postponed != nullptr, "Baked template data must be initialized");

			std::string result = "[";
			for (usize i = 0; i < postponed->template_arguments_symbols.size(); i++) {
				if (i != 0) result += ", ";
				const auto value
					= ctx.query<helios::QueryConstValueOf>(postponed->template_arguments_symbols[i])
				          .valueOrThrow();
				if (const auto type = value.get<SymbolType<>>(); type.has_value())
					result += renderSymbolTypeName(ctx, type.value()).text;
				else
					result += value.toString();
			}
			return result + "]";
		}

		std::string sourceClassName(query::Context& ctx, const helios::SymID symbol) {
			std::vector<std::string> path;
			auto                     element = helios::maybeSymbolPst(symbol);
			while (element.has_value()) {
				auto unlocked = element.value().unlock(ctx);
				if (unlocked->getElementKind() == pst::ElementKind::Namespace
				    || unlocked->getElementKind() == pst::ElementKind::Class) {
					if (auto statement = unlocked.dynamicCast<pst::Stmt>(); statement.has_value()) {
						if (auto identifier = statement.value()->getDeclSymbolIdentifier();
						    identifier.has_value())
							path.emplace_back(identifier.value().unlock(ctx)->unwrap().str());
					}
				}

				if (unlocked->getElementKind() == pst::ElementKind::TemplateStmt
				    && unlocked->hasAdditionalRootData()
				    && std::holds_alternative<pst::AdditionalRootData::BakedTemplateParent>(
						unlocked->getAdditionalRootData().pst_parent
					)) {
					const auto& baked_parent
						= std::get<pst::AdditionalRootData::BakedTemplateParent>(
							unlocked->getAdditionalRootData().pst_parent
						);
					const auto bake_data
						= base::anyCast<helios::templates::TemplateBakePSTLinkedData>(
							baked_parent.template_bake_data
						);
					CORE_ASSERT(!path.empty(), "A baked class template must have a class name");
					path.back() += templateArgumentsSuffix(ctx, bake_data);
					element = bake_data.pst_parent_element;
					continue;
				}

				auto parent = helios::getPSTElementParent(ctx, unlocked);
				element     = parent.isLangElement()
				                ? base::Optional(parent.getAsLangElement())
				                : base::Optional<pst::AccessLocked<pst::LangElement>>{};
			}

			std::ranges::reverse(path);
			const auto result = base::strJoin(path, ".");
			return result.empty() ? helios::name(symbol).str() : result;
		}

		std::string joinSourceTypeNames(
			query::Context&                  ctx,
			const std::vector<SymbolType<>>& types,
			std::string_view                 separator,
			const TypeNamePrecedence         child_precedence = TypeNamePrecedence::Variant
		) {
			std::string result;
			for (usize i = 0; i < types.size(); i++) {
				if (i != 0) result += separator;
				result += renderSymbolTypeName(ctx, types[i]).nestedIn(child_precedence);
			}
			return result;
		}

		SourceTypeName renderAbstractTypeName(query::Context& ctx, const AbstractType type) {
			switch (type.getKind()) {
			case Kind::Pointer:
				return {
					.text = "ptr "
					      + renderSymbolTypeName(ctx, type.as<PointerAbstractType>().getPointee())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case Kind::ManyPointer:
				return {
					.text
					= "manyptr "
					+ renderSymbolTypeName(ctx, type.as<ManyPointerAbstractType>().getPointee())
					      .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case Kind::CPointer:
				return {
					.text = "cptr "
					      + renderSymbolTypeName(ctx, type.as<CPointerAbstractType>().getPointee())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case Kind::Slice: {
				const auto element = type.as<SliceAbstractType>().getElementType();
				if (element == SymbolType<>::withDefaults(getCharType()))
					return { .text = "str", .precedence = TypeNamePrecedence::Atomic };
				return {
					.text = "slice "
					      + renderSymbolTypeName(ctx, element).nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			}
			case Kind::StaticArray: {
				auto        array   = type.as<StaticArrayAbstractType>();
				std::string extents = base::strConcat("[", array.getSize(), "]");
				auto        element = array.getElementType();
				while (element.getType().getKind() == Kind::StaticArray
				       && element.getRefKind() == ReferenceKind::Direct) {
					array = element.getType().as<StaticArrayAbstractType>();
					extents += base::strConcat("[", array.getSize(), "]");
					element = array.getElementType();
				}
				return {
					.text = renderSymbolTypeName(ctx, element).nestedIn(TypeNamePrecedence::Postfix)
					      + extents,
					.precedence = TypeNamePrecedence::Postfix,
				};
			}
			case Kind::Tuple: {
				const auto components = type.as<TupleAbstractType>().getComponents();
				return {
					.text = "(" + joinSourceTypeNames(ctx, components, ", ")
					      + (components.size() == 1 ? ",)" : ")"),
					.precedence = TypeNamePrecedence::Atomic,
				};
			}
			case Kind::Variant:
				return {
					.text = joinSourceTypeNames(
						ctx,
						type.as<VariantAbstractType>().getUnderlyingTypes(),
						" | ",
						TypeNamePrecedence::Prefix
					),
					.precedence = TypeNamePrecedence::Variant,
				};
			case Kind::Class:
				return {
					.text       = sourceClassName(ctx, type.as<ClassAbstractType>().getSymbol()),
					.precedence = TypeNamePrecedence::Atomic,
				};
			default:
				return { .text = type.toString(), .precedence = TypeNamePrecedence::Atomic };
			}
		}

		SourceTypeName renderSymbolTypeName(query::Context& ctx, const SymbolType<> type) {
			using enum ReferenceKind;
			const std::string prefix = base::strConcat(
				type.getUniqueness() == Uniqueness::Unique ? "unique " : "",
				type.getLeakage() == Leakage::Leaking ? "leaking " : "",
				type.getMutability() == Mutability::Immutable ? "const " : "",
				type.getRefKind() == Direct ? ""
				: type.getRefKind() == Box  ? "box "
											: "ref "
			);
			auto inner = renderAbstractTypeName(ctx, type.getType());
			if (prefix.empty()) return inner;
			return {
				.text       = prefix + inner.nestedIn(TypeNamePrecedence::Prefix),
				.precedence = TypeNamePrecedence::Prefix,
			};
		}
	}

	std::string AbstractType::sourceTypeName(query::Context& ctx) const {
		return renderAbstractTypeName(ctx, *this).text;
	}

	std::string detail::sourceTypeName(query::Context& ctx, const SymbolType<AbstractType>& type) {
		return renderSymbolTypeName(ctx, type).text;
	}
}

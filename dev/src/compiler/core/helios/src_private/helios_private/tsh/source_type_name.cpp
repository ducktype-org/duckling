// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <ctv/numeric_value.hpp>
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

#include <algorithm>
#include <format>
#include <ranges>
#include <type_traits>

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

		std::string renderCompileTimeValue(query::Context& ctx, const ctv::CompileTimeValue& value) {
			if (const auto type = value.get<SymbolType<>>(); type.has_value())
				return renderSymbolTypeName(ctx, type.value()).text;
			if (const auto number = value.get<numeric_value::NumericValue>(); number.has_value())
				return std::visit(
					[&](const auto stored) -> std::string {
						if constexpr (std::is_floating_point_v<decltype(stored)>)
							return std::format(
								"{}{}",
								stored,
								std::is_same_v<decltype(stored), const f32> ? "f32" : "f64"
							);
						else
							return number->toString();
					},
					number->getStorage()
				);
			return value.toString();
		}

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
				result += renderCompileTimeValue(ctx, value);
			}
			return result + "]";
		}

		std::string sourceDeclarationName(query::Context& ctx, const helios::SymID symbol) {
			std::vector<std::string> path;
			auto                     element = helios::maybeSymbolPst(symbol);
			if (element.has_value()) {
				auto unlocked = element.value().unlock(ctx);
				if (unlocked->getElementKind() == pst::ElementKind::TemplateStmt) {
					element
						= unlocked.dynamicCast<pst::TemplateStmt>().value()->getInnerStatement();
					const auto inner_kind = element.value().unlock(ctx)->getElementKind();
					if (inner_kind != pst::ElementKind::Class
					    && inner_kind != pst::ElementKind::Namespace)
						path.emplace_back(helios::name(symbol).str());
				}
			}
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
			const TypeNamePrecedence         parent_precedence = TypeNamePrecedence::Variant
		) {
			std::string result;
			for (usize i = 0; i < types.size(); i++) {
				if (i != 0) result += separator;
				result += renderSymbolTypeName(ctx, types[i]).nestedIn(parent_precedence);
			}
			return result;
		}

		SourceTypeName renderAbstractTypeName(query::Context& ctx, const AbstractType type) {
			switch (type.getKind()) {
			case Kind::Unit:
				return { .text = "()", .precedence = TypeNamePrecedence::Atomic };
			case Kind::Void:
				return { .text = "void", .precedence = TypeNamePrecedence::Atomic };
			case Kind::Byte:
				return { .text = "byte", .precedence = TypeNamePrecedence::Atomic };
			case Kind::Bool:
				return { .text = "bool", .precedence = TypeNamePrecedence::Atomic };
			case Kind::Char:
				return { .text = "char", .precedence = TypeNamePrecedence::Atomic };
			case Kind::Integral: {
				const auto integral = type.as<IntegralAbstractType>();
				return {
					.text = base::strConcat(
						integral.getSignedness() == IntegralAbstractType::Signedness::Signed ? "i"
																							 : "u",
						integral.getSize().asInt()
					),
					.precedence = TypeNamePrecedence::Atomic,
				};
			}
			case Kind::Float:
				return {
					.text = base::strConcat("f", type.as<FloatAbstractType>().getSize().asInt()),
					.precedence = TypeNamePrecedence::Atomic,
				};
			case Kind::Meta:
				return { .text = "type", .precedence = TypeNamePrecedence::Atomic };
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
				if (element == SymbolType<>::withDefaultsMut(getCharType()))
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
					.text = sourceDeclarationName(ctx, type.as<ClassAbstractType>().getSymbol()),
					.precedence = TypeNamePrecedence::Atomic,
				};
			case Kind::Function: {
				const auto function = type.as<FunctionAbstractType>();
				return {
					.text = "Function ("
					      + joinSourceTypeNames(ctx, function.getParameterTypes(), ", ") + ") -> ("
					      + renderSymbolTypeName(ctx, function.getResultType()).text + ")",
					.precedence = TypeNamePrecedence::Atomic,
				};
			}
			case Kind::TypeTemplate:
				return {
					.text
					= "Type template: "
					+ sourceDeclarationName(ctx, type.as<TypeTemplateAbstractType>().getSource()),
					.precedence = TypeNamePrecedence::Atomic,
				};
			case Kind::RawPointer:
			case Kind::Namespace:
			case Kind::Module:
			case Kind::Import:
				// These compiler-level kinds do not currently have a Duckling type-expression
				// spelling. Keep their existing human-readable representation until one is defined.
				return { .text = type.toString(), .precedence = TypeNamePrecedence::Atomic };
			case Kind::COUNT:
				CORE_UNREACHABLE();
			}
			CORE_UNREACHABLE();
		}

		SourceTypeName renderSymbolTypeName(query::Context& ctx, const SymbolType<> type) {
			using enum ReferenceKind;
			const std::string prefix = base::strConcat(
				type.getUniqueness() == Uniqueness::Unique ? "unique " : "",
				type.getLeakage() == Leakage::Leaking ? "leaking " : "",
				type.getMutability() == Mutability::Immutable ? "imm " : "",
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

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	std::string SymbolType<ABSTRACT_TYPE>::sourceTypeName(query::Context& ctx) const {
		return renderSymbolTypeName(ctx, SymbolType<AbstractType>(*this)).text;
	}

#define INSTANTIATE_SOURCE_TYPE_NAME(TYPE) \
	template std::string SymbolType<TYPE>::sourceTypeName(query::Context&) const;
	INSTANTIATE_SOURCE_TYPE_NAME(AbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(UnitAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(VoidAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(ByteAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(BoolAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(CharAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(IntegralAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(FloatAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(RawPointerAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(PointerAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(ManyPointerAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(CPointerAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(SliceAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(TupleAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(FunctionAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(StaticArrayAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(VariantAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(ClassAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(NamespaceAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(ModuleAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(MetaAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(ImportAbstractType)
	INSTANTIATE_SOURCE_TYPE_NAME(TypeTemplateAbstractType)
#undef INSTANTIATE_SOURCE_TYPE_NAME
}

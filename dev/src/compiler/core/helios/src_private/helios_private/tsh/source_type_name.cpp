// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <ctv/numeric_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios/tsh/value_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/tsh/source_type_name.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <type_traits>

namespace compiler::tsh {
	namespace {
		std::string renderCompileTimeValue(query::Context& ctx, const ctv::CompileTimeValue& value) {
			if (const auto type = value.get<ValueType<>>(); type.has_value())
				return renderValueTypeName(ctx, type.value()).text;
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

		std::string sourceSymbolName(query::Context& ctx, const helios::SymID symbol) {
			auto result = helios::name(symbol).str();
			if_opt_some(helios::getSymRef(symbol)->maybe_template_instance_data, data) {
				result
					+= "["
				     + base::strJoin(
						   data.template_arguments_symbols
							   | std::views::transform([&](const helios::SymID argument) {
									 return renderCompileTimeValue(
										 ctx,
										 helios::getSymRef(argument)
											 ->getGeneratedData<helios::defgen::GeneratedConstant>()
											 ->value
									 );
								 }),
						   ", "
					 )
				     + "]";
			}
			return result;
		}

		std::string sourceDeclarationName(query::Context& ctx, const helios::SymID symbol) {
			std::vector<std::string> path{ sourceSymbolName(ctx, symbol) };

			auto element = helios::maybeSymbolPst(symbol);
			while (element.has_value()) {
				auto parent = helios::getPSTElementParent(ctx, element.value().unlock(ctx));
				element     = parent.isLangElement()
				                ? base::Optional(parent.getAsLangElement())
				                : base::Optional<pst::AccessLocked<pst::LangElement>>{};
				if (not element.has_value()) break;

				auto unlocked = element.value().unlock(ctx);
				if (unlocked->getElementKind() != pst::ElementKind::Namespace
				    && unlocked->getElementKind() != pst::ElementKind::Class)
					continue;

				const auto declaration = ctx.query<helios::QuerySymbolOfSTMT>(
												{ unlocked.dynamicCast<pst::Stmt>().value() }
				)
				                             .valueOrThrow();
				path.emplace_back(sourceSymbolName(ctx, declaration));
			}

			std::ranges::reverse(path);
			return base::strJoin(path, ".");
		}

		std::string joinSourceTypeNames(
			query::Context&                 ctx,
			const std::vector<ValueType<>>& types,
			std::string_view                separator,
			const TypeNamePrecedence        parent_precedence = TypeNamePrecedence::Function
		) {
			std::string result;
			for (usize i = 0; i < types.size(); i++) {
				if (i != 0) result += separator;
				result += renderValueTypeName(ctx, types[i]).nestedIn(parent_precedence);
			}
			return result;
		}
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
					integral.getSignedness() == IntegralAbstractType::Signedness::Signed ? "i" : "u",
					integral.getSize().asInt()
				),
				.precedence = TypeNamePrecedence::Atomic,
			};
		}
		case Kind::Float:
			return {
				.text       = base::strConcat("f", type.as<FloatAbstractType>().getSize().asInt()),
				.precedence = TypeNamePrecedence::Atomic,
			};
		case Kind::Meta:
			return { .text = "type", .precedence = TypeNamePrecedence::Atomic };
		case Kind::Pointer:
			return {
				.text = "ptr "
				      + renderValueTypeName(ctx, type.as<PointerAbstractType>().getPointee())
				            .nestedIn(TypeNamePrecedence::Prefix),
				.precedence = TypeNamePrecedence::Prefix,
			};
		case Kind::ManyPointer:
			return {
				.text = "manyptr "
				      + renderValueTypeName(ctx, type.as<ManyPointerAbstractType>().getPointee())
				            .nestedIn(TypeNamePrecedence::Prefix),
				.precedence = TypeNamePrecedence::Prefix,
			};
		case Kind::CPointer:
			return {
				.text = "cptr "
				      + renderValueTypeName(ctx, type.as<CPointerAbstractType>().getPointee())
				            .nestedIn(TypeNamePrecedence::Prefix),
				.precedence = TypeNamePrecedence::Prefix,
			};
		case Kind::Slice: {
			const auto element = type.as<SliceAbstractType>().getElementType();
			if (element == ValueType<>::withDefaultsMut(getCharType()))
				return { .text = "str", .precedence = TypeNamePrecedence::Atomic };
			return {
				.text
				= "slice " + renderValueTypeName(ctx, element).nestedIn(TypeNamePrecedence::Prefix),
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
				.text
				= renderValueTypeName(ctx, element).nestedIn(TypeNamePrecedence::Postfix) + extents,
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
		case Kind::Variant: {
			const auto variant = type.as<VariantAbstractType>();
			if (variant.representsOptionalType())
				return {
					.text = "?"
					      + renderValueTypeName(ctx, variant.getOptionalValueType())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			return {
				.text = joinSourceTypeNames(
					ctx, variant.getUnderlyingTypes(), " | ", TypeNamePrecedence::Prefix
				),
				.precedence = TypeNamePrecedence::Variant,
			};
		}
		case Kind::Class:
			return {
				.text       = sourceDeclarationName(ctx, type.as<ClassAbstractType>().getSymbol()),
				.precedence = TypeNamePrecedence::Atomic,
			};
		case Kind::Function: {
			const auto function = type.as<FunctionAbstractType>();
			return {
				.text = base::strConcat(
					function.getFunctionKind() == FunctionKind::Raw ? "funptr(" : "(",
					joinSourceTypeNames(ctx, function.getParameterTypes(), ", "),
					") -> ",
					renderValueTypeName(ctx, function.getResultType())
						.nestedIn(TypeNamePrecedence::Function)
				),
				.precedence = TypeNamePrecedence::Function,
			};
		}
		case Kind::TypeTemplate:
			return {
				.text = "Type template: "
				      + sourceDeclarationName(ctx, type.as<TypeTemplateAbstractType>().getSource()),
				.precedence = TypeNamePrecedence::Atomic,
			};
		case Kind::RawPointer:
			return { .text = "raw_pointer", .precedence = TypeNamePrecedence::Atomic };
		case Kind::Namespace:
			return { .text = "namespace", .precedence = TypeNamePrecedence::Atomic };
		case Kind::Module:
			return { .text = "module", .precedence = TypeNamePrecedence::Atomic };
		case Kind::Import:
			return { .text = "import", .precedence = TypeNamePrecedence::Atomic };
		case Kind::COUNT:
			CORE_UNREACHABLE();
		}
		CORE_UNREACHABLE();
	}

	SourceTypeName renderValueTypeName(query::Context& ctx, const ValueType<> type) {
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

	std::string AbstractType::sourceTypeName(query::Context& ctx) const {
		return renderAbstractTypeName(ctx, *this).text;
	}
}

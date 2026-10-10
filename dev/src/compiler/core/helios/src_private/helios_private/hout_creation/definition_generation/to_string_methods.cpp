// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "to_string_methods.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/template_stmt.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/except/exceptions.hpp>
#include <base/misc/anycast.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

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

		SourceTypeName sourceTypeName(query::Context& ctx, tsh::SymbolType<> type);

		std::string templateArgumentsSuffix(
			query::Context& ctx, const templates::TemplateBakePSTLinkedData& bake_data
		) {
			const auto postponed = bake_data.postponed_data->load(std::memory_order_acquire);
			CORE_ASSERT(postponed != nullptr, "Baked template data must be initialized");

			std::string result = "[";
			for (usize i = 0; i < postponed->template_arguments_symbols.size(); i++) {
				if (i != 0) result += ", ";
				const auto value
					= ctx.query<QueryConstValueOf>(postponed->template_arguments_symbols[i])
				          .valueOrThrow();
				if (const auto type = value.get<tsh::SymbolType<>>(); type.has_value())
					result += sourceTypeName(ctx, type.value()).text;
				else
					result += value.toString();
			}
			return result + "]";
		}

		std::string sourceClassName(query::Context& ctx, const SymID symbol) {
			std::vector<std::string> path;
			auto                     element = maybeSymbolPst(symbol);
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
					const auto bake_data = base::anyCast<templates::TemplateBakePSTLinkedData>(
						baked_parent.template_bake_data
					);
					CORE_ASSERT(!path.empty(), "A baked class template must have a class name");
					path.back() += templateArgumentsSuffix(ctx, bake_data);
					element = bake_data.pst_parent_element;
					continue;
				}

				auto parent = getPSTElementParent(ctx, unlocked);
				element     = parent.isLangElement()
				                ? base::Optional(parent.getAsLangElement())
				                : base::Optional<pst::AccessLocked<pst::LangElement>>{};
			}

			std::ranges::reverse(path);
			std::string result;
			for (const auto& part: path) {
				if (!result.empty()) result += ".";
				result += part;
			}
			return result.empty() ? name(symbol).str() : result;
		}

		std::string joinSourceTypeNames(
			query::Context&                       ctx,
			const std::vector<tsh::SymbolType<>>& types,
			std::string_view                      separator,
			const TypeNamePrecedence              child_precedence = TypeNamePrecedence::Variant
		) {
			std::string result;
			for (usize i = 0; i < types.size(); i++) {
				if (i != 0) result += separator;
				result += sourceTypeName(ctx, types[i]).nestedIn(child_precedence);
			}
			return result;
		}

		SourceTypeName sourceAbstractTypeName(query::Context& ctx, const tsh::AbstractType type) {
			switch (type.getKind()) {
			case tsh::Kind::Pointer:
				return {
					.text = "ptr "
					      + sourceTypeName(ctx, type.as<tsh::PointerAbstractType>().getPointee())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case tsh::Kind::ManyPointer:
				return {
					.text = "manyptr "
					      + sourceTypeName(ctx, type.as<tsh::ManyPointerAbstractType>().getPointee())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case tsh::Kind::CPointer:
				return {
					.text = "cptr "
					      + sourceTypeName(ctx, type.as<tsh::CPointerAbstractType>().getPointee())
					            .nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			case tsh::Kind::Slice: {
				const auto element = type.as<tsh::SliceAbstractType>().getElementType();
				if (element == tsh::SymbolType<>::withDefaults(tsh::getCharType()))
					return { .text = "str", .precedence = TypeNamePrecedence::Atomic };
				return {
					.text
					= "slice " + sourceTypeName(ctx, element).nestedIn(TypeNamePrecedence::Prefix),
					.precedence = TypeNamePrecedence::Prefix,
				};
			}
			case tsh::Kind::StaticArray: {
				auto        array   = type.as<tsh::StaticArrayAbstractType>();
				std::string extents = base::strConcat("[", array.getSize(), "]");
				auto        element = array.getElementType();
				while (element.getType().getKind() == tsh::Kind::StaticArray
				       && element.getRefKind() == tsh::ReferenceKind::Direct) {
					array = element.getType().as<tsh::StaticArrayAbstractType>();
					extents += base::strConcat("[", array.getSize(), "]");
					element = array.getElementType();
				}
				return {
					.text
					= sourceTypeName(ctx, element).nestedIn(TypeNamePrecedence::Postfix) + extents,
					.precedence = TypeNamePrecedence::Postfix,
				};
			}
			case tsh::Kind::Tuple: {
				const auto components = type.as<tsh::TupleAbstractType>().getComponents();
				return {
					.text = "(" + joinSourceTypeNames(ctx, components, ", ")
					      + (components.size() == 1 ? ",)" : ")"),
					.precedence = TypeNamePrecedence::Atomic,
				};
			}
			case tsh::Kind::Variant:
				return {
					.text = joinSourceTypeNames(
						ctx,
						type.as<tsh::VariantAbstractType>().getUnderlyingTypes(),
						" | ",
						TypeNamePrecedence::Prefix
					),
					.precedence = TypeNamePrecedence::Variant,
				};
			case tsh::Kind::Class:
				return {
					.text = sourceClassName(ctx, type.as<tsh::ClassAbstractType>().getSymbol()),
					.precedence = TypeNamePrecedence::Atomic,
				};
			default:
				return { .text = type.toString(), .precedence = TypeNamePrecedence::Atomic };
			}
		}

		SourceTypeName sourceTypeName(query::Context& ctx, const tsh::SymbolType<> type) {
			using enum tsh::ReferenceKind;
			const std::string prefix = base::strConcat(
				type.getUniqueness() == tsh::Uniqueness::Unique ? "unique " : "",
				type.getLeakage() == tsh::Leakage::Leaking ? "leaking " : "",
				type.getMutability() == tsh::Mutability::Immutable ? "const " : "",
				type.getRefKind() == Direct ? ""
				: type.getRefKind() == Box  ? "box "
											: "ref "
			);
			auto inner = sourceAbstractTypeName(ctx, type.getType());
			if (prefix.empty()) return inner;
			return {
				.text       = prefix + inner.nestedIn(TypeNamePrecedence::Prefix),
				.precedence = TypeNamePrecedence::Prefix,
			};
		}

		bool containsRuntimeMeta(
			query::Context&                 ctx,
			const tsh::AbstractType         type,
			std::vector<tsh::AbstractType>& visited
		) {
			if (type.getKind() == tsh::Kind::Meta) return true;
			if (std::ranges::find(visited, type) != visited.end()) return false;
			visited.emplace_back(type);

			auto symbol_type_contains_meta = [&](const tsh::SymbolType<> component) {
				const auto component_type = component.getType();
				if (component_type.getKind() == tsh::Kind::Meta) return true;

				const auto method
					= getSymRef(toStringSymForType(ctx, component_type))->getDataOpt<Method>();
				return method.has_value() && method.value()->kind == Method::Kind::ToString
				    && containsRuntimeMeta(ctx, component_type, visited);
			};

			switch (type.getKind()) {
			case tsh::Kind::Slice:
				return symbol_type_contains_meta(type.as<tsh::SliceAbstractType>().getElementType());
			case tsh::Kind::StaticArray:
				return symbol_type_contains_meta(
					type.as<tsh::StaticArrayAbstractType>().getElementType()
				);
			case tsh::Kind::Tuple:
				return std::ranges::any_of(
					type.as<tsh::TupleAbstractType>().getComponents(), symbol_type_contains_meta
				);
			case tsh::Kind::Variant:
				return std::ranges::any_of(
					type.as<tsh::VariantAbstractType>().getUnderlyingTypes(),
					symbol_type_contains_meta
				);
			case tsh::Kind::Class:
				return std::ranges::any_of(
					type.getInterface(ctx)->getFieldsView(),
					[&](const tsh::InterfaceElement& field) {
						return symbol_type_contains_meta(field.getType(ctx));
					}
				);
			default:
				return false;
			}
		}

		bool containsRuntimeMeta(query::Context& ctx, const tsh::AbstractType type) {
			std::vector<tsh::AbstractType> visited;
			return containsRuntimeMeta(ctx, type, visited);
		}
	}

	/**
	 * @brief Get the symbol of the `toString` method for a given type.
	 */
	SymID toStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		const auto to_string
			= type.getInterface(ctx)->getSpecialElement(tsh::MemberSpecialKind::ToString);
		CORE_ASSERT(to_string.has_value(), "Every symbol should have toString.");
		return to_string.value()->getSymbol();
	}

	SymID generatedToStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("toString"),
			.generated_symbol_data = Method{ .owner_type = type, .kind = Method::Kind::ToString },
		});
	}

	query::QResult<Box<code::Expr>> toStringExpr(
		query::Context& ctx, Box<code::Expr> value, code::ElementOrigin callee_origin
	) {
		const auto      value_type = value->expression_type.getType();
		const Shorthand s{ ctx };

		if (value_type.getKind() == tsh::Kind::Meta) {
			UNPACK_QRESULT(
				auto compile_time_value =, ctx.query<QueryEvaluateHOUTExpression>({ value.ref() })
			);
			const auto represented_type = compile_time_value.get<tsh::SymbolType<>>();
			CORE_ASSERT(
				represented_type.has_value(),
				"An expression of the meta type must evaluate to a symbol type"
			);
			return s.litStrObj(base::StrID(sourceTypeName(ctx, represented_type.value()).text));
		}

		const auto to_string_sym = toStringSymForType(ctx, value_type);
		if (const auto method = getSymRef(to_string_sym)->getDataOpt<Method>();
		    method.has_value() && method.value()->kind == Method::Kind::ToString
		    && containsRuntimeMeta(ctx, value_type)) {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"A `type` value cannot be stringified at runtime.", callee_origin.getStablePosition()
			));
			return query::Failed();
		}
		return s.call(
			withOrigin(callee_origin, s.ident(to_string_sym)), s.prepToPassSelf(std::move(value))
		);
	}

#define STRING_TYPE tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx))
#define LANG_PRIMITIVE(primitive) \
	ctx.query<QueryLanguagePrimitiveSymID>({ primitive })->valueOrThrow()

	SymID stringAppendMethodSym(query::Context& ctx, const bool arg_by_reference) {
		// Look up the `append` method on the `String` class. `append` is overloaded on a `String`
		// argument, so we select either the by-reference (`append(other: ref String)`) or the
		// by-value (`append(other: String)`) overload depending on `arg_by_reference`.
		auto       string_abstract_type = tsh::getStringType(ctx);
		const auto expected_arg_type
			= tsh::SymbolType<>::withDefaults(string_abstract_type)
		          .withReferenceKind(
					  arg_by_reference ? tsh::ReferenceKind::Ref : tsh::ReferenceKind::Direct
				  );
		const auto& lookup_result = HInterface::ofTypeInstance(string_abstract_type)
		                                .lookup(ctx, base::StrID("append"))
		                                ->valueOrThrow();


		for (const SymID candidate: lookup_result.leaves) {
			const auto fn_type = ctx.query<QueryTypeOfSymbol>(candidate)
			                         ->valueOrThrow()
			                         .getType()
			                         .as<tsh::FunctionAbstractType>();
			const auto& params = fn_type.getParameterTypes();
			if (params.size() == 2 && params.at(1) == expected_arg_type) return candidate;
		}
		ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
			base::strConcat(
				"The String class language primitive doesn't have the `append(",
				arg_by_reference ? "ref " : "",
				"String)` method."
			),
			"This method is required for the compiler to work."
		));
		query::throwFailed();
		CORE_UNREACHABLE();
	}

	struct IMPLEMENT_QUERY(QueryToStringMethod, query::QResult<HOUTFunction>) {
		static void stringifyBool(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param  = to_string_decl.parameters.at(0).helios_symbol;
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyBool);

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		static void stringifyIntegral(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param      = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_int_type = to_string_decl.parameters.at(0).type;
			const bool is_signed
				= source_int_type.getType().as<tsh::IntegralAbstractType>().getSignedness()
			   == tsh::IntegralAbstractType::Signedness::Signed;
			const auto target_int_type = tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
				ctx,
				64,
				is_signed ? tsh::IntegralAbstractType::Signedness::Signed
						  : tsh::IntegralAbstractType::Signedness::Unsigned
			));
			const auto builtin_sym     = LANG_PRIMITIVE(
                is_signed ? LanguagePrimitive::StringifyI64 : LanguagePrimitive::StringifyU64
			);

			const Shorthand s{ ctx };
			body.emplace_back(
				s.ret(s.call(s.ident(builtin_sym), s.coerce(s.ident(self_param), target_int_type)))
			);
		}

		static void stringifyPointer(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body,
			const bool                     is_many_pointer
		) {
			const auto  self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto& self_type  = to_string_decl.parameters.at(0).type.getType();
			const auto  pointee    = is_many_pointer
			                           ? tsh::ManyPointerAbstractType(self_type).getPointee()
			                           : tsh::PointerAbstractType(self_type).getPointee();
			// Both primitives are templated on the pointee, so they have to be baked before
			// they can be called.
			const auto builtin_sym = bakeLanguagePrimitive(
				ctx,
				is_many_pointer ? LanguagePrimitive::StringifyManyPtr
								: LanguagePrimitive::StringifyPtr,
				{ pointee }
			);

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		static void stringifyFloat(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto target_float_type
				= tsh::SymbolType<>::withDefaults(tsh::getFloatType(ctx, 64));
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyF64);

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(
				s.call(s.ident(builtin_sym), s.coerce(s.ident(self_param), target_float_type))
			));
		}

		static void stringifyChar(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param  = to_string_decl.parameters.at(0).helios_symbol;
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyChar);

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		static void stringifySlice(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			auto& self_param         = to_string_decl.parameters.at(0);
			auto  slice_type         = self_param.type.getType().as<tsh::SliceAbstractType>();
			auto  slice_element_type = slice_type.getElementType();
			// A `str` is text, not a sequence of characters, so it keeps its own primitive.
			const SymID callee_sym
				= (slice_element_type.getType() == tsh::getCharType()
			       and slice_element_type.getRefKind() == tsh::ReferenceKind::Direct)
			        ? LANG_PRIMITIVE(LanguagePrimitive::StringifyStr)
			        : bakeLanguagePrimitive(
						  ctx, LanguagePrimitive::StringifySlice, { slice_element_type }
					  );

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(callee_sym), s.ident(self_param.helios_symbol))));
		}

		static void stringifyCPointer(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto  self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto& self_type  = to_string_decl.parameters.at(0).type.getType();
			const auto  pointee    = tsh::CPointerAbstractType(self_type).getPointee();
			const auto  builtin_sym
				= bakeLanguagePrimitive(ctx, LanguagePrimitive::StringifyCPtr, { pointee });

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		/**
		 * @brief Stringifies a static array with the element-wise primitive, baked on the array
		 * type and its length.
		 */
		static void stringifyStaticArray(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto array_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::StaticArrayAbstractType>();
			const auto size
				= numeric_value::NumericValue::createOfType(
					  tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
					  array_type.getSize()
				)
			          .expect("The length of a static array always fits an i64");
			const auto builtin_sym = bakeLanguagePrimitive(
				ctx,
				LanguagePrimitive::StringifyStaticArray,
				{ tsh::SymbolType<>::withDefaults(array_type), size }
			);

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		/**
		 * @brief Stringifies a variant as the text of the value it currently holds.
		 *
		 * Every alternative gets a case, so the match is exhaustive without a wildcard. Each case
		 * binds the payload by reference and defers to that type's own `toString`.
		 */
		static void stringifyVariant(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const Shorthand s{ ctx };
			const SymID     self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto      variant_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::VariantAbstractType>();
			const auto& alternatives = variant_type.getUnderlyingTypes();

			std::vector<code::MatchExpr::Case> cases;
			for (usize i = 0; i < alternatives.size(); i++) {
				// A binding is always a reference to the payload, which for a `box` alternative
				// is a reference to the pointee.
				const auto payload_type = alternatives[i]
				                              .withReferenceKind(tsh::ReferenceKind::Ref)
				                              .withMutability(tsh::Mutability::Mutable);

				const SymID payload_sym = ctx.query<QueryGeneratedSymbol>({
					.name = base::StrID(base::strConcat("__alternative_", i)),
					.generated_symbol_data
					= GeneratedFunctionVariable{ .function_symbol = to_string_decl.original_symbol,
				                                 .variable_index  = i,
				                                 .type            = payload_type },
				});

				const SymID payload_to_string = toStringSymForType(ctx, alternatives[i].getType());

				cases.emplace_back(Shorthand::matchCase(
					i,
					payload_type,
					payload_sym,
					s.call(s.ident(payload_to_string), s.prepToPassSelf(s.ident(payload_sym)))
				));
			}

			// `self` is already a reference to the variant, which is what the match wants.
			body.emplace_back(s.ret(s.matchExpr(s.ident(self_param), std::move(cases))));
		}

		static void stringifyUnit(
			Context& ctx,
			const HOUTFunctionDeclaration& /* to_string_decl */,
			std::vector<Box<code::Stmt>>& body
		) {
			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.litStrObj(base::StrID("()"))));
		}

		static void stringifyAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body,
			const base::StrID              prefix,
			const CRef<tsh::TypeInterface> type_interface
		) {
			const Shorthand s{ ctx };
			const auto      append_sym  = stringAppendMethodSym(ctx, true);
			const SymID     self_symbol = to_string_decl.parameters.at(0).helios_symbol;
			auto            deref_self  = [&] { return s.deref(s.ident(self_symbol)); };

			// Prelude: the class name and opening parenthesis
			const auto result_sym = ctx.query<QueryGeneratedSymbol>(
				{ .name = base::StrID("__result"),
			      .generated_symbol_data
			      = GeneratedFunctionVariable{ .function_symbol = to_string_decl.original_symbol,
			                                   .variable_index  = 0,
			                                   .type            = STRING_TYPE } }
			);
			body.emplace_back(s.var(result_sym, STRING_TYPE, s.litStrObj(base::StrID(prefix))));

			// Main body: append the fields
			const std::vector<tsh::InterfaceElement> fields
				= type_interface->getFieldsView() | std::ranges::to<std::vector>();
			const auto num_fields = fields.size();

			for (usize idx = 0; idx < num_fields; idx++) {
				// Get the stringified field
				const auto& field               = fields.at(idx);
				const auto  field_type          = field.getType(ctx);
				const SymID field_to_string_sym = toStringSymForType(ctx, field_type.getType());

				Box<code::Expr> accessed_field
					= s.prepToPassSelf(s.access(deref_self(), field.getSymbol()));

				auto stringified_field
					= s.call(s.ident(field_to_string_sym), std::move(accessed_field));

				// Append the stringified field.
				body.emplace_back(s.expr(s.call(
					s.ident(append_sym),
					s.refOf(s.ident(result_sym)),
					s.refOf(std::move(stringified_field))
				)));

				// Append the separator or closing parenthesis.
				body.emplace_back(s.expr(s.call(
					s.ident(append_sym),
					s.refOf(s.ident(result_sym)),
					s.refOf(s.litStrObj(base::StrID(idx < num_fields - 1 ? "," : ")")))
				)));
			}

			// Finally, return
			body.emplace_back(s.ret(s.ident(result_sym)));
		}

		static void stringifyTuple(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto tuple_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::TupleAbstractType>();
			const auto tuple_interface = tuple_type.getInterface(ctx);

			stringifyAggregate(ctx, to_string_decl, body, base::StrID("("), tuple_interface);
		}

		static void stringifyClass(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto class_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::ClassAbstractType>();
			const auto class_name      = name(class_type.getSymbol());
			const auto class_interface = class_type.getInterface(ctx);

			stringifyAggregate(
				ctx, to_string_decl, body, base::StrID(class_name.str() + "("), class_interface
			);
		}

		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto  to_string_sym  = toStringSymForType(ctx, owner_type);
			const auto& to_string_decl = ctx.query<QueryDeclOfFun>(to_string_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Integral: {
				stringifyIntegral(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Float: {
				stringifyFloat(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Pointer: {
				stringifyPointer(ctx, to_string_decl, body, false);
				break;
			}
			case tsh::Kind::ManyPointer: {
				stringifyPointer(ctx, to_string_decl, body, true);
				break;
			}
			case tsh::Kind::Char: {
				stringifyChar(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Bool: {
				stringifyBool(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Slice: {
				stringifySlice(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Unit: {
				stringifyUnit(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Tuple: {
				stringifyTuple(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::CPointer: {
				stringifyCPointer(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::StaticArray: {
				stringifyStaticArray(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Variant: {
				stringifyVariant(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Class: {
				stringifyClass(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Meta: {
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					"A `type` value cannot be stringified at runtime."
				));
				const Shorthand s{ ctx };
				body.emplace_back(s.ret(s.litStrObj(base::StrID(""))));
				break;
			}
			default: {
				const Shorthand s{ ctx };
				std::string     msg
					= "Stringification not yet implemented for " + owner_type.toString();
				body.emplace_back(s.ret(s.litStrObj(base::StrID(msg))));
				break;
			}
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&to_string_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryToStringMethod);
}

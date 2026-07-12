#include "symbols.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/utils/hout_walkers.hpp>
#include <helios_private/attributes/backend_dependent.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>

#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <functional>
#include <unordered_set>
#include <vector>

namespace compiler::helios {
	bool implementsQueryCodeOfFun(SymID id) {
		// Builtins have FunctionDeclaration kind, but can implement code of fun.
		if (kind(id) != SymbolKind::Function && kind(id) != SymbolKind::Method
		    && kind(id) != SymbolKind::FunctionDeclaration)
			return false;

		variant_match(getSymRef(id)->other) {
			variant_case_novalue(PstImplementedSemantics) { return true; }
			variant_case(BuiltinSemantics, data) {
				// Only if implemented in HOUT, then it can be called with code of fun.
				return getBuiltinOrigins(data.builtin).contains(BuiltinOrigin::HOUT);
			}
			variant_case_novalue(GENERATED_SYMBOL_SEMANTICS_LIST) {
				return kind(id) != SymbolKind::FunctionDeclaration;
			}
			variant_default { CORE_PANIC("Not implemented."); }
		}
		CORE_UNREACHABLE();
	}

	EmissionPolicy emissionPolicy(query::Context& ctx, SymID id) {
		variant_match(getSymRef(id)->other) {
			variant_case(PstImplementedSemantics, pst_data) { 
				// This is a very simple, and very suboptimal heuristic for now, we can improve it later if needed.
				// @TODO: #2996 change it to a better implementation.
				// Symbol should just know this!
				auto pst_element_ancestor =
					getPSTElementParent( ctx, pst_data.getElement().unlock(ctx));
				while (pst_element_ancestor.isLangElement()) {
					auto element = pst_element_ancestor.getAsLangElement().unlock(ctx);
					if (element->getElementKind()
					    == pst::ElementKind::TemplateStmt) {

						// Symbols from within templates, are (probably) replicated
						return EmissionPolicy::Replicated;
					}
					pst_element_ancestor = getPSTElementParent(ctx, element);
				}
				
				return EmissionPolicy::OwnerOnly;
			}
			variant_case(BuiltinSemantics, data) {
				// Only if HOUT implements the builtin we want to replicate it.
				return getBuiltinOrigins(data.builtin).contains(BuiltinOrigin::HOUT)
				         ? EmissionPolicy::Replicated
				         : EmissionPolicy::OwnerOnly;
			}
			variant_case_novalue(
				defgen::BuiltinOperator, defgen::BoxBuiltin, defgen::ScriptMainWrapper
			) {
				return EmissionPolicy::OwnerOnly;
			}
			variant_case_novalue(
				defgen::Constructor,
				defgen::Method,
				defgen::Parameter,
				defgen::SelfParameter,
				defgen::Field,
				defgen::GeneratedFunctionVariable,
				defgen::ControlFlowLocal,
				defgen::ReplExpressionWrapper,
				defgen::ReplInstructionWrapper
			) {
				return EmissionPolicy::Replicated;
			}
			variant_default { CORE_PANIC("Unhandled symbol semantics"); }
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Query "linked-scope", that is scope
	 * that "lookup in" operation will perform lookup.
	 *
	 * @note For HELIOS internal use only
	 * @note It is a partial-Query. It won't work for all symbol
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryLinkedScope, SymID, query::QResult<ScopeID>, ({}));

	bool isWildcard(SymID id) { return getSymRef(id)->common.is_wildcard; }

	bool isAlias(SymID id) { return getSymRef(id)->common.is_alias; }

	bool isIgnoredByLookup(SymID id) { return getSymRef(id)->common.is_ignored_by_lookup; }

	base::StrID name(SymID id) { return getSymRef(id)->common.name; }

	// @TODO: #895 Reevaluate this helper when entry points become explicit.
	bool isGlobalFun(SymID id) {
		CORE_ASSERT(
			getSymRef(id)->common.kind == SymbolKind::FunctionDeclaration
				|| getSymRef(id)->common.kind == SymbolKind::Function,
			"Not a function."
		);

		return scopeDepth(scope(id)) == 1;
	}

	bool isGlobalVar(query::Context& ctx, SymID id) {
		CORE_ASSERT(getSymRef(id)->common.kind == SymbolKind::Variable, "Not a variable.");

		// For now if the symbol doesn't have a PST element it is not global,
		// Maybe be not true if we have generated globals in the future.
		if_opt_none(getSymRef(id)->maybePstElement()) return false;

		// We go up the PST until we find a statement that determines whether the variable is global
		// or not.
		return std::invoke(
			[&ctx](this auto self, const pst::Access<pst::LangElement>& el) -> bool {
				switch (el->getElementKind()) {
				// Variables inside Top-level and namespace are global:
				case pst::ElementKind::TopLevel:
				case pst::ElementKind::Namespace:
					return true;

				// Variables inside classes, functions and some statements are not global:
				case pst::ElementKind::Class:
				case pst::ElementKind::Fun:
				case pst::ElementKind::ClassMethod:
				case pst::ElementKind::ClassSpecial:
				case pst::ElementKind::ClassSpecifierBlock:
				case pst::ElementKind::If:
				case pst::ElementKind::While:
				case pst::ElementKind::For:
					return false;

				// For other elements we go up the PST tree:
				case pst::ElementKind::CodeBlock:
				case pst::ElementKind::CodeBlockOrStmt:
				case pst::ElementKind::Variable:
				case pst::ElementKind::Expand:
				case pst::ElementKind::StmtSpecifier:
				case pst::ElementKind::SpecifierBlock:
				case pst::ElementKind::Block:
				case pst::ElementKind::ExprElement:
				case pst::ElementKind::ExprHolder:
				case pst::ElementKind::ExprStmt:
				case pst::ElementKind::IdentifierWrapper: {
					auto pst_parent = getPSTElementParent(ctx, el);

					CORE_ASSERT(
						pst_parent.isLangElement(),
						"Non TopLevel elements should always have a pst or an expand parent"
					);
					return self(pst_parent.getAsLangElement().unlock(ctx));
				}
				default:
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"Global variable detection is not implemented for variables inside "
							"elements of kind: ",
							el->elementType()
						),
						el->getStablePosition()
					));
					query::throwFailed();
					CORE_UNREACHABLE();
				}
			},
			getSymRef(id)->maybePstElement().value().unlock(ctx)
		);
	}

	base::Optional<BuiltinKind> isBuiltin(SymID id) {
		if (const auto builtin = getSymRef(id)->getDataOpt<BuiltinSemantics>())
			return builtin.value()->builtin;
		// Box alloc/free functions are backend-implemented builtins too.
		if (const auto box = getSymRef(id)->getDataOpt<defgen::BoxBuiltin>())
			return box.value()->kind == defgen::BoxBuiltin::Kind::Alloc ? BuiltinKind::BoxAlloc
			                                                            : BuiltinKind::BoxFree;
		return {};
	}

	SymbolKind kind(SymID id) { return getSymRef(id)->common.kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->getScope().value(); }

	base::Optional<ScopeID> maybeScope(SymID id) { return getSymRef(id)->getScope(); }

	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context& ctx, SymID id) {
		return getSymRef(id)->stmtCast(ctx);
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> maybeSymbolPst(SymID id) {
		return getSymRef(id)->maybePstElement();
	}

	template<typename Attribute>
	bool hasAttribute(SymID id) {
		return std::ranges::any_of(getSymRef(id)->common.attributes, [](auto a) {
			return base::holds<Attribute>(a);
		});
	}

#define MAKE_ATTR_INSTANCE(attr) template bool hasAttribute<attr>(SymID id);
	FOR_EACH(MAKE_ATTR_INSTANCE, ATTRIBUTES_LIST)

	std::string prettyDebugPrint(SymID sym, query::Context& ctx) {
		// Short summary
		// 1. Get the symbol's PST element
		// 2. If the element is a statement get its name
		// 3. Get the parent of the pst element
		// 4. Repeat until we reach the root element
		// 5. Concatenate all names with " -> "
		// 6. Prepend the module name

		std::string                                         out = "";
		base::Optional<pst::AccessLocked<pst::LangElement>> pst = maybeSymbolPst(sym);
		if (not pst) return name(sym).str();

		do {
			if (!pst.value().unlock(ctx)->getParent()) break;
			auto stmt = pst->unlock(ctx).dynamicCast<pst::Stmt>();
			if (!stmt) continue;

			auto name = stmt.value()->getDeclSymbolIdentifier();
			if (!name.has_value()) continue;

			auto id = name->unlock(ctx)->unwrap();

			if (!out.empty())
				out = base::strConcat(id, " -> ", out);
			else
				out = id.str();

			// Get the parent of the current pst element
		} while ((pst = pst.value().unlock(ctx)->getParent()));
		auto module_name = compiler::frontend::moduleName(module(scope(sym)));
		out              = base::strConcat(module_name.str(), " -> ", out);

		return out;
	}

	std::vector<Attribute> attributesFromPSTStatement(
		query::Context& ctx, pst::Access<pst::Stmt> stmt
	) {
		std::vector<Attribute> result;
		for (auto attr_locked: stmt->getAttributes()) {
			auto pst_attr       = attr_locked.unlock(ctx);
			auto pst_attr_value = pst_attr->getName().unlock(ctx);

			if (pst_attr_value->numberOfNames() != 1) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Attribute is not supported", pst_attr_value->getStablePosition()
				));
				query::throwFailed();
			}
			auto ident = pst_attr_value->getNameByIndex(0);
			auto name  = ident.unlock(ctx)->unwrap();

			// Pass the attribute argument list (e.g. the "ptr_from_slice" in
			// @builtin("ptr_from_slice")) to the per-attribute parser, empty when there is no
			// `(...)`. Each parser validates and consumes the PST arguments itself.
			base::Optional<pst::AccessLocked<pst::AtrArgList>> args;
			if (pst_attr->hasArgs()) args = pst_attr->getArgs();

			auto attr_opt = attrFromStr(ctx, name, args);
			if_opt_none(attr_opt) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					base::strConcat("Attribute name '", name, "' is not recognized"),
					pst_attr_value->getStablePosition()
				));
				query::throwFailed();
			}

			auto attr = attr_opt.value();

			if (not isValidForStmt(attr, stmt->getStmtKind())) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Attribute is not supported on this type of statement.",
					pst_attr_value->getStablePosition()
				));
				query::throwFailed();
			}

			result.emplace_back(attr);
		}

		auto validation_result = validateAttributes(result);
		if (not validation_result.has_value()) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				validation_result.error(), stmt->getStablePosition()
			));
		}

		return result;
	}

	/**
	 * @brief SymbolData Factory.
	 * Make symbols from PST statements.
	 * @todo in the future this function should not use dynamic_casts,
	 * and should be merged with makeSymbolFromPSTElement.
	 * It should use a visitor, or only depend on StmtKind and virtual methods.
	 *
	 * @param scope
	 * @param stmt
	 * @return Ref<SymbolData>
	 */
	query::QResult<SymbolData> makeSymbolFromStatement(
		query::Context& ctx, ScopeID scope, pst::Access<pst::Stmt> stmt
	) {
		// @TODO: change this function to visitor to avoid dynamic_casts


		// Attribute handling
		auto attributes           = attributesFromPSTStatement(ctx, stmt);
		bool is_ignored_by_lookup = std::ranges::any_of(attributes, &disablesLookup);

		// Check if it is a builtin.
		if_opt_some(getAttrInVector<attributes::Builtin>(attributes), builtin_data) {
			auto function = stmt.dynamicCast<pst::FunDecl>().value();
			return SymbolData::makeBuiltinSymbolData(
				{
					.name                 = function->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::FunctionDeclaration,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),

				},
				BuiltinSemantics{
					scope,
					stmt->getHash(),
					builtin_data->builtin,
				}
			);
		}

		PstImplementedSemantics pst_data(scope, stmt->getHash());

		switch (stmt->getStmtKind()) {
		case pst::StmtKind::Fun: {
			auto function = stmt.dynamicCast<pst::Fun>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = function->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Function,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::FunDecl: {
			auto function = stmt.dynamicCast<pst::FunDecl>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = function->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::FunctionDeclaration,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Namespace: {
			auto namespace_stmt = stmt.dynamicCast<pst::Namespace>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = namespace_stmt->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Namespace,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Const: {
			auto const_stmt = stmt.dynamicCast<pst::Const>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = const_stmt->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Const,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Class: {
			auto class_stmt = stmt.dynamicCast<pst::Class>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = class_stmt->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Class,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Alias: {
			auto alias = stmt.dynamicCast<pst::Alias>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = alias->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Alias,
					.is_alias             = true,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Using: {
			auto  using_stmt  = stmt.dynamicCast<pst::Using>().value();
			auto  pointed     = using_stmt->getPointed().unlock(ctx);
			bool  is_wildcard = pointed->getStar();
			usize size        = pointed->numberOfNames();

			std::vector<tpc::Identifier> target(size);
			for (usize i = 0; i < size; i++)
				target[i] = { .value = pointed->getNameByIndex(i).unlock(ctx)->unwrap() };

			return SymbolData::makePSTSymbolData(
				{
					.name
					= is_wildcard
			            ? base::StrID(
							  base::strConcat("<WILDCARD USING> ", target.front().value).c_str()
						  )
			            : target.back().value,
					.kind                 = SymbolKind::Using,
					.is_wildcard          = is_wildcard,
					.is_alias             = true,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Variable: {
			auto variable = stmt.dynamicCast<pst::Variable>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = variable->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Variable,
					.is_wildcard          = false,
					.is_alias             = false,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Import: {
			// @TODO: #2791 finish this, note that this may require bigger refactor to unify the
			// logic with other similar constructs (e.g. usings), and because imports may introduce
			// multiple names now. We could extend wildcard machinery to keep the general assumption
			// of one-stmt=one-symbol while handling the above.
			auto import       = stmt.dynamicCast<pst::Import>().value();
			auto import_chain = import->getImportChain().unlock(ctx);
			if (auto import_as = import_chain.dynamicCast<pst::ImportIdentifierAs>()) {
				base::StrID name;
				if (import_as.value()->isImportAs())
					name = import_as.value()->asWhat().value().unlock(ctx)->unwrap();
				else {
					usize count = import_as.value()->numberOfNames();
					name = import_as.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				}

				return SymbolData::makePSTSymbolData(
					{
						.name                 = name,
						.kind                 = SymbolKind::Import,
						.is_wildcard          = false,
						.is_alias             = false,
						.is_ignored_by_lookup = is_ignored_by_lookup,
						.attributes           = std::move(attributes),
					},
					pst_data
				);
			} else if (auto import_star = import_chain.dynamicCast<pst::ImportStarHides>()) {
				// import a.b.c.*;
				usize count = import_star.value()->numberOfNames();
				auto  name  = import_star.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				if (import_star.value()->isImportHides()) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Import chains of type `ImportStarHides` are not yet supported in "
						"makeSymbolFromStatement",
						import_star.value()->getStablePosition()
					));
				}
				return SymbolData::makePSTSymbolData(
					{
						.name                 = name,
						.kind                 = SymbolKind::Import,
						.is_wildcard          = true,
						.is_alias             = false,
						.is_ignored_by_lookup = is_ignored_by_lookup,
						.attributes           = std::move(attributes),
					},
					pst_data
				);
			} else if (auto import_nested = import_chain.dynamicCast<pst::ImportNested>()) {
				// import a.b.c(...);
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Import chains of type `ImportNested` are not yet supported in "
					"makeSymbolFromStatement",
					import_nested.value()->getStablePosition()
				));
				usize count = import_nested.value()->numberOfNames();
				auto  name = import_nested.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				return SymbolData::makePSTSymbolData(
					{
						.name                 = name,
						.kind                 = SymbolKind::Import,
						.is_wildcard          = false,
						.is_alias             = false,
						.is_ignored_by_lookup = is_ignored_by_lookup,
						.attributes           = std::move(attributes),
					},
					pst_data
				);
			} else {
				throw base::NotYetImplemented(
					"Not handled type of import chain in makeSymbolFromStatement"
				);
			}
		}
		case pst::StmtKind::Method: {
			auto method = stmt.dynamicCast<pst::Method>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = method->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Method,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Field: {
			auto field = stmt.dynamicCast<pst::Field>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = field->getName().unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::Field,
					.dependent            = true,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Constructor: {
			auto constructor = stmt.dynamicCast<pst::Constructor>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name                 = constructor->getIdentifier()
			                                  ? constructor->getIdentifier()->unlock(ctx)->unwrap()
			                                  : base::StrID("create"),
					.kind                 = SymbolKind::Constructor,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::CopyConstructor: {
			return SymbolData::makePSTSymbolData(
				{
					.name                 = lang_def::keywordToStr(lang_def::Keyword::Copy),
					.kind                 = SymbolKind::Constructor,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::Destructor: {
			return SymbolData::makePSTSymbolData(
				{
					.name                 = base::StrID("destroy"),
					.kind                 = SymbolKind::Destructor,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::CodeDecl: {
			// CodeDecl include things like named ifs, whiles, fors and code blocks.
			// Note that this function should only be called if the statement creates a symbol, so
			// we can assume that it is only named ones.
			return SymbolData::makePSTSymbolData(
				{
					.name                 = stmt->getDeclSymbolIdentifier()->unlock(ctx)->unwrap(),
					.kind                 = SymbolKind::NamedCodeElement,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		case pst::StmtKind::TemplateStmt: {
			auto template_decl   = stmt.dynamicCast<pst::TemplateStmt>().value();
			auto inner_statement = template_decl->getInnerStatement().unlock(ctx);

			auto inner_statement_kind = inner_statement->getStmtKind();
			if (inner_statement_kind != pst::StmtKind::Fun
			    && inner_statement_kind != pst::StmtKind::FunDecl
			    && inner_statement_kind != pst::StmtKind::Class
			    && inner_statement_kind != pst::StmtKind::Namespace
			    && inner_statement_kind != pst::StmtKind::Const) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Templates are only supported for functions, classes, namespaces and consts",
					template_decl->getStablePosition()
				));
				return query::Failed();
			}

			return SymbolData::makePSTSymbolData(
				{
					.name = template_decl->getDeclSymbolIdentifier()->unlock(ctx)->unwrap(),
					.kind = SymbolKind::Template,
					.is_ignored_by_lookup = is_ignored_by_lookup,
					.attributes           = std::move(attributes),
				},
				pst_data
			);
		}
		default:
			break;
		}
		[[maybe_unused]] auto stmt_ptr = &*stmt;
		CORE_PANIC(base::strConcat(
			"makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt_ptr).name()
		));
	}

	/**
	 * @brief SymbolData Factory,
	 * Make symbols from PST elements other then statements.
	 * @todo in the future this function should not use dynamic_casts,
	 * and should be merged with makeSymbolFromStatement.
	 */
	SymbolData makeSymbolFromPSTElement(
		ScopeID scope, pst::Access<pst::LangElement> element, query::Context& ctx
	) {
		if (auto parameter_opt = element.dynamicCast<pst::Param>()) {
			auto parameter = parameter_opt.value();
			return SymbolData::makePSTSymbolData(
				{
					.name = parameter->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Parameter,
				},
				PstImplementedSemantics(scope, element->getHash())
			);
		}

		if (auto ident_wrapper_opt = element.dynamicCast<pst::IdentifierWrapper>()) {
			auto ident_wrapper = ident_wrapper_opt.value();
			auto parent_opt    = ident_wrapper->getParent();
			CORE_ASSERT(parent_opt.has_value(), "IdentifierWrapper without parent");

			auto parent_elem = parent_opt.value().unlock(ctx);
			if (auto for_parent_opt = parent_elem.dynamicCast<pst::For>()) {
				return SymbolData::makePSTSymbolData(
					{
						.name = ident_wrapper->unwrap(),
						.kind = SymbolKind::Variable,
					},
					PstImplementedSemantics(scope, element->getHash())
				);
			} else {
				CORE_PANIC("IdentifierWrapper in QuerySymbolOfStmt with unsupported parent");
			}
		}

		CORE_PANIC("Not handled PST element in makeSymbolFromPSTElement");
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, query::QResult<SymbolData>) {
		/**
		 * @brief Return the scope, that symbol created from given PST element
		 * Should be in.
		 * @note This has to be consistent with QuerySymbolsInScope
		 * @TODO: #2407 this could take unlocked Access
		 */
		static ScopeID getPSTElementParentScope(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> element
		) {
			// Note: This has to be consistent with QuerySymbolsInScope logic.
			// @TODO: #2397 maybe move it into a single place

			auto unlocked = element.unlock(ctx);
			auto parent   = getPSTElementParent(ctx, unlocked);

			// We should never hit an element without PST parent here,
			// since it would be a non-expand root element (i.e. TopLevel element), and those don't
			// have symbols.
			CORE_ASSERT(
				parent.isLangElement(), "PST element without LangElement parent in QuerySymbolOfSTMT"
			);
			return ctx.query<QueryPrimaryCodeScopeFor>(parent.getAsLangElement());
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto scope = getPSTElementParentScope(ctx, key.element);
			if (auto stmt = key.element.unlock(ctx).dynamicCast<pst::Stmt>())
				return PResult{ makeSymbolFromStatement(ctx, scope, stmt.value()).valueOrThrow() };
			else
				return PResult{ makeSymbolFromPSTElement(scope, key.element.unlock(ctx), ctx) };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA([](CRef<query::QResult<SymbolData>> presult) -> QResult {
			if (presult->hasFailed())
				return query::Failed();
			else
				return SymID{ &presult->valueOrPanic() };
		})

	private:
		/**
		 * @brief This is a helper function for getAllHeliosSymbols.
		 * Use only inside that function (and only for debug/test purposes)!
		 */
		static std::vector<SymID> getAllCachedSymbols() {
			// This implementation is fragile, adjust if needed.

			std::vector<SymID> out;

			for (auto& [key, cache_entry]: cache) {
				if (cache_entry.data.hasFailed()) continue;

				out.emplace_back(SymID{ &cache_entry.data.valueOrPanic() });
			}
			return out;
		}

		// for getAllCachedSymbols:
		friend std::vector<SymID> compiler::helios::getAllHeliosSymbols();
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolOfSTMT);

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, query::QResult<LookupResult>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->common.kind) {
			case SymbolKind::Using:
			case SymbolKind::Namespace:
			case SymbolKind::Import: {
				// @NOTE: for now imports are done via linked scope that looks at root
				// module scope, but in the future it might be changed to custom code

				// @note: this will probably brake for usings,
				// when they look at a symbol without linked scope.
				// We might just delete QueryLinkedScope at some point,
				// when QueryLookupInSymbol will get more and more
				// per-symbol-kind cases.

				UNPACK_QRESULT(auto linked_scope =, ctx.query<QueryLinkedScope>(key.symbol));
				return *HInterface::ofScope(linked_scope)
				            .lookup(ctx, key.name, { key.follow_wildcards });
			}

			// @note: here case for variables will be calling TS
			default:
				throw base::NotYetImplemented(
					base::strConcat("Lookup in symbol: ", key.symbol.ref->common.name)
				);
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInSymbol);

	struct IMPLEMENT_QUERY(QueryLinkedScope, query::QResult<ScopeID>) {
		struct QueryLinkedScopeVisitor final: pst::PstVisitorEmpty {
			query::Context& ctx;
			QKey            key;

			QueryLinkedScopeVisitor(query::Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::Optional<query::QResult<ScopeID>> result_scope;

			void output(query::QResult<ScopeID> out) {
				CORE_ASSERT(result_scope.empty(), "Output already set");
				result_scope.emplace(out);
			}

			void visitUsing(pst::Access<pst::Using> using_stmt) final {
				auto                         pointed = using_stmt->getPointed().unlock(ctx);
				usize                        size    = pointed->numberOfNames();
				std::vector<tpc::Identifier> pointed_to_names(size);
				for (usize i = 0; i < size; i++)
					pointed_to_names[i]
						= { .value = pointed->getNameByIndex(i).unlock(ctx)->unwrap() };

				auto lookup_res = lookupChain(
					ctx,
					LookupChainKey{ .names       = pointed_to_names,
				                    .begin_scope = scope(key),
				                    .params      = { .with_wildcards = false } }
				);
				CORE_ASSERT(
					lookup_res.hasValue() && not lookup_res.valueOrThrow().empty(),
					"Using points to something that does not exists or is empty"
				);
				auto ret = ctx.query<QueryLinkedScope>({ lookup_res.valueOrThrow().back() });
				output(ret);
			}

			void visitImport(pst::Access<pst::Import> import_stmt) final {
				// @TODO: proper error handling

				auto import_stmt_ptr = import_stmt.dynamicCast<pst::Import>().value();
				auto import_chain    = import_stmt_ptr->getImportChain().unlock(ctx);

				std::vector<base::StrID> module_path;

				auto unlock_all_names = [&](auto import_chain_casted) {
					usize                    size = import_chain_casted->numberOfNames();
					std::vector<base::StrID> names(size);
					for (usize i = 0; i < size; i++)
						names[i] = import_chain_casted->getNameByIndex(i).unlock(ctx)->unwrap();
					return names;
				};

				// Handle different import chain types
				if (auto import_as = import_chain.dynamicCast<pst::ImportIdentifierAs>()) {
					auto names  = unlock_all_names(import_as.value());
					module_path = std::vector<base::StrID>{ names.begin(), names.end() };
				} else if (auto import_star = import_chain.dynamicCast<pst::ImportStarHides>()) {
					auto names  = unlock_all_names(import_star.value());
					module_path = std::vector<base::StrID>{ names.begin(), names.end() };
				} else {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Unknown import chain type.", import_stmt->getStablePosition()
					));
					output(query::Failed());
					return;
				}

				auto maybe_imported_module
					= frontend::getRelativeModule(ctx, module(scope(key)), module_path);

				if (!maybe_imported_module.has_value()) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Module not found.", import_stmt->getStablePosition()
					));
					output(query::Failed());
					return;
				}

				// Here we don't access just root scope, because root scopes are currently empty:
				auto linked_scope
					= queryRootScopeOfMainModuleFile(ctx, maybe_imported_module.value());

				output(linked_scope);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.ref->common.kind) {
			case SymbolKind::Namespace:
				return queryBodyCodeScopeFor(ctx, key.ref->stmtCast(ctx).value());


			// Special cases for "wildcards":
			case SymbolKind::Using:
			case SymbolKind::Import: {
				QueryLinkedScopeVisitor visitor(ctx, key);
				key.ref->maybePstElement().value().unlock(ctx)->acceptVisitor(visitor);
				return visitor.result_scope.value();
			}
			default: {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"Linked scope for this symbol kind is not implemented yet: ",
						key.ref->common.kind
					),
					stmt(ctx, key.ref).value()->getStablePosition()
				));
				return query::Failed();
			}
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLinkedScope);

	base::Bit256 KeyOf_LookupInSymbol::queryUnstablePerfectHash() const {
		auto hash_1 = symbol.queryUnstablePerfectHash();
		auto hash_2 = std::hash<base::StrID>()(name);

		return { hash_1, hash_2, static_cast<u64>(follow_wildcards) };
	}

	struct IMPLEMENT_QUERY(QueryDealias, QueryDealias_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<tpc::Identifier> pointed_chain;
			if (kind(key) == SymbolKind::Using) {
				auto dotted = getSymRef(key)
				                  ->maybePstElement()
				                  .value()
				                  .unlock(ctx)
				                  .dynamicCast<pst::Using>()
				                  .value()
				                  ->getPointed()
				                  .unlock(ctx);
				pointed_chain.resize(dotted->numberOfNames());
				for (usize i = 0; i < dotted->numberOfNames(); i++)
					pointed_chain[i] = { .value = dotted->getNameByIndex(i).unlock(ctx)->unwrap() };
			} else if (kind(key) == SymbolKind::Alias) {
				auto dotted = getSymRef(key)
				                  ->maybePstElement()
				                  .value()
				                  .unlock(ctx)
				                  .dynamicCast<pst::Alias>()
				                  .value()
				                  ->getPointed()
				                  .unlock(ctx);
				pointed_chain.resize(dotted->numberOfNames());
				for (usize i = 0; i < dotted->numberOfNames(); i++)
					pointed_chain[i] = { .value = dotted->getNameByIndex(i).unlock(ctx)->unwrap() };
			} else {
				return SymbolList{ { key } };
			}

			UNPACK_QRESULT_MOVE(
				auto lookup_chain =,
				lookupChain(
					ctx, LookupChainKey{ pointed_chain, scope(key), { .with_wildcards = false } }
				)
			);

			return lookup_chain;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	struct IMPLEMENT_QUERY(QueryConstValueOf, query::QResult<ctv::CompileTimeValue>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			// Get the const's data

			variant_match(getSymRef(key)->other) {
				variant_case(defgen::GeneratedConstant, const_data) { return const_data.value; }

				variant_case(PstImplementedSemantics, pst_data) {
					const auto pst
						= pst_data.getElement().unlock(ctx).dynamicCast<pst::Const>().value();
					const auto type = ctx.query<QueryTypeOfSymbol>(key)->valueOrThrow();

					// Get the coerced HOUT expression
					const auto hout_qresult = getHoutOfExprWithExpectedType(
						ctx, pst->getValue().value().unlock(ctx)->getExpr(), type
					);
					if (hout_qresult.hasFailed()) return query::Failed();

					// Evaluate the HOUT expression at compile-time
					auto ctv = ctx.query<QueryEvaluateHOUTExpression>(
						{ hout_qresult.valueOrThrow().ref() }
					);
					if (ctv.hasFailed()) return query::Failed();
					return ctv.valueOrThrow();
				}

				variant_default {
					CORE_PANIC("Unexpected SymbolData::other type in QueryConstValueOf");
				}
			}

			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	struct IMPLEMENT_QUERY(QuerySpecifiersOfSymbol, QuerySpecifiersOfSymbol_Result) {
		// @TODO: #1321 do not unlock whole elements, checking the type of the parent would be enough

		/**
		 * @brief Check if the ancestors of PST element `el` match the provided kinds in order,
		 * and return the ancestor if they do.
		 */
		template<typename... Kinds>
		static base::Optional<pst::Access<pst::LangElement>> getAncestor(
			query::Context& ctx, pst::Access<pst::LangElement> el, Kinds... kinds
		) {
			return getAncestorImpl(ctx, el, kinds...);
		}

		// Base case: no more kinds to check → success
		static base::Optional<pst::Access<pst::LangElement>> getAncestorImpl(
			query::Context&, pst::Access<pst::LangElement> el
		) {
			return el;
		}

		// Recursive case: check current kind, then move up
		template<typename... Rest>
		static base::Optional<pst::Access<pst::LangElement>> getAncestorImpl(
			query::Context&               ctx,
			pst::Access<pst::LangElement> el,
			pst::ElementKind              expected,
			Rest... rest
		) {
			if (auto parent = el->getParent()) {
				auto parent_el = parent.value().unlock(ctx);
				if (parent_el->getElementKind() == expected)
					return getAncestorImpl(ctx, parent_el, rest...);
			}
			return {};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<pst::AccessLocked<pst::StmtSpecifier>> specifiers;
			if (!std::holds_alternative<PstImplementedSemantics>(getSymRef(key)->other)) {
				// Generated symbols have no specifiers (for now)
				return {};
			}

			auto pst_element = getSymRef(key)->maybePstElement().value().unlock(ctx);

			// SpecifierBlock only has a "CodeBlock" child, which can has "Stmt" children.
			//
			// The Class situation is a bit more complicated
			// @TODO: #1535 Fix/figure out class handling
			// Might be deprecated by merging the two cases (?) #2961
			while (true) {
				if (auto as_stmt = pst_element.dynamicCast<pst::Stmt>()) {
					// Can swap to append range when g++ 15 is more commonly available
					auto to_add = as_stmt.value()->getSpecifiers();
					specifiers.insert(
						specifiers.end(),
						std::make_move_iterator(to_add.begin()),
						std::make_move_iterator(to_add.end())
					);
				}
				if (auto result_stmt = getAncestor(
						ctx, pst_element, pst::ElementKind::CodeBlock, pst::ElementKind::SpecifierBlock
					)) {
					pst_element = *std::move(result_stmt);
				} else {
					break;
				}
			}

			return specifiers;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySpecifiersOfSymbol);

	namespace defgen {
		base::Bit256 KeyFor_QueryGeneratedSymbol::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				std::hash<base::StrID>()(name),
				generatedSymbolUnstablePerfectHash(generated_symbol_data)
			);
		}

		struct IMPLEMENT_QUERY(QueryGeneratedSymbol, SymbolData) {
			static auto provide(Context&, const QKey& key) -> PResult {
				return SymbolData::makeGeneratedSymbolData(key.name, key.generated_symbol_data);
			}

			QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK

		private:
			/**
			 * @brief This is a helper function for getAllHeliosSymbols.
			 * Use only inside that function (and only for debug/test purposes)!
			 */
			static std::vector<SymID> getAllCachedSymbols() {
				// This implementation is fragile, adjust if needed.

				std::vector<SymID> out;

				for (auto& [key, cache_entry]: cache)
					out.emplace_back(QResult{ &cache_entry.data });
				return out;
			}

			// for getAllCachedSymbols:
			friend std::vector<SymID> compiler::helios::getAllHeliosSymbols();
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(QueryGeneratedSymbol);
	}

	struct IMPLEMENT_QUERY(QueryDirectFunctionCalls, query::QResult<std::vector<SymID>>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				isFunctionLike(kind(key)),
				"Query function dependencies called on non-function symbol"
			);

			if (kind(key) == SymbolKind::FunctionDeclaration) {
				// For function declarations we check if a function declaration is a backend
				// dependent symbol.
				if (hasAttribute<attributes::BackendDependent>(key)) {
					// If yes then we append all the results from all implementations.
					return getBackendDependentImplementations(ctx, key);
				} else {
					return {};
				}
			}

			if (not implementsQueryCodeOfFun(key)) return {};

			const auto& fun_hout_result = ctx.query<QueryCodeOfFun>(key)->valueOrThrow();
			return code::collectCalledSymbols(fun_hout_result);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDirectFunctionCalls);

	struct IMPLEMENT_QUERY(QueryTransitiveFunctionCalls, query::QResult<std::vector<SymID>>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function || kind(key) == SymbolKind::FunctionDeclaration,
				"Query transitive function dependencies called on non-function symbol"
			);

			std::vector<SymID>        worklist;
			std::unordered_set<SymID> visited_functions;
			std::vector<SymID>        all_dependencies;

			worklist.push_back(key);  // Insert root function SymID.
			visited_functions.insert(key);

			while (!worklist.empty()) {
				SymID current_func = worklist.back();
				worklist.pop_back();

				all_dependencies.push_back(current_func);

				Ref direct_dependencies
					= &ctx.query<QueryDirectFunctionCalls>(current_func)->valueOrThrow();

				for (const SymID& dependency: *direct_dependencies) {
					if (!visited_functions.contains(dependency)) {
						visited_functions.insert(dependency);
						worklist.push_back(dependency);
					}
				}
			}

			base::filterVectorInPlace(all_dependencies, [](auto sym) {
				return kind(sym) != SymbolKind::FunctionDeclaration;
			});
			return all_dependencies;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTransitiveFunctionCalls);

	std::vector<SymID> getAllHeliosSymbols() {
		// this implementation is fragile, adjust if needed

		CORE_ASSERT(
			!query::Context::areWeInsideQuery(), "getAllHeliosSymbols called from within query!"
		);

		auto pst_symbols = ImplementationOf_QuerySymbolOfSTMT::getAllCachedSymbols();
		auto generated_symbols
			= defgen::ImplementationOf_QueryGeneratedSymbol::getAllCachedSymbols();

		std::vector<SymID> output;
		output.reserve(pst_symbols.size() + generated_symbols.size());

		output.insert(output.end(), pst_symbols.begin(), pst_symbols.end());
		output.insert(output.end(), generated_symbols.begin(), generated_symbols.end());

		return output;
	}
}

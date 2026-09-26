#include "scopes.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/access.hpp>
#include <frontend/packages/standard_packages.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/import.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/using.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/desugaring/for.hpp>
#include <helios_private/hout_creation/desugaring/match.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/pst_layer/for_all.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/stable_container.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/query_failed_try.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>

namespace compiler::helios {


	auto getScopeRef(ScopeID id) { return ScopeAccess_Functor::get(id); }

	base::Optional<ScopeID> parent(ScopeID id) {
		auto ref = getScopeRef(id);
		if (ref->is_root) {
			return {};
		} else {
			CORE_ASSERT(ref->parent.has_value(), "Non root scope has no parent.");
			return ref->parent.value();
		}
	}

	frontend::ModuleID module(ScopeID id) { return getScopeRef(id)->parent_module; }

	u64 scopeDepth(ScopeID id) { return getScopeRef(id)->depth; }

	/**
	 * @brief A way helios creates scope for given pst element.
	 */
	enum class ElementScopeKind {
		// Has standard scope:
		Standard,

		// Inherits scope from its parent:
		Transparent,

		// Gets parent scope of what transparent scope would be.
		// This is used for expand expressions, where the lookup should happen higher than the
		// expand statement itself.
		ParentTransparent,

		// Does not have a scope:
		Invalid,

		// @TODO: introduce:
		// * TransparentInvalid -- transparent for implementation, but invalid for user
		// this allows to easily implement scope parents, but disallow to get PrimaryScopes for
		// elements that don't have it.
		// For examples namespaces don't have primary scopes (which can be counterintuitive)
	};

	/**
	 * Determines scope kind for given PST element.
	 * @TODO: #2407 this could take unlocked access
	 */
	ElementScopeKind getScopeKind(query::Context& ctx, pst::AccessLocked<pst::LangElement> locked) {
		// @TODO: move it to different file?
		auto element = locked.unlock(ctx);

		switch (element->getElementKind()) {
		case pst::ElementKind::TopLevel:
			return ElementScopeKind::Standard;

		case pst::ElementKind::Import:
		case pst::ElementKind::Selector:
		case pst::ElementKind::SelectorList:
		case pst::ElementKind::NestedSelectorList:
		case pst::ElementKind::DottedName:
		// I don't know if this is correct
		case pst::ElementKind::StmtSpecifier:
			return ElementScopeKind::Invalid;


		// code blocks:
		case pst::ElementKind::CodeBlock: {
			auto parent_kind = element->getParent().value().unlock(ctx)->getElementKind();
			if (parent_kind == pst::ElementKind::CodeBlockOrStmt
			    || parent_kind == pst::ElementKind::SpecifierBlock)
				return ElementScopeKind::Transparent;
			else
				return ElementScopeKind::Standard;
		}
		case pst::ElementKind::CodeBlockOrStmt:
			return ElementScopeKind::Standard;

		// I don't know if this is correct
		case pst::ElementKind::SpecifierBlock:
			return ElementScopeKind::Transparent;


		case pst::ElementKind::Namespace:
		case pst::ElementKind::Variable:
		case pst::ElementKind::Using:
		case pst::ElementKind::Const:
		case pst::ElementKind::Class:
		case pst::ElementKind::Action:
		case pst::ElementKind::Block:  //< note that Block != CodeBlock
		case pst::ElementKind::ClassField:
		case pst::ElementKind::CallArgument:
		case pst::ElementKind::Attribute:
			// this is transparent, since we don't need this scope:
			return ElementScopeKind::Transparent;

		// this has to be transparent, since ClassBlock scopes
		// contain all symbols in AccessBlock's
		case pst::ElementKind::ClassSpecifierBlock:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::If:
		case pst::ElementKind::While:
		case pst::ElementKind::For:
		case pst::ElementKind::Fun:
		case pst::ElementKind::FunDecl:
		case pst::ElementKind::ClassMethod:
		case pst::ElementKind::ClassSpecial:
			return ElementScopeKind::Standard;

		// The match owns the generated subject local; each case owns the symbols bound
		// by its pattern.
		case pst::ElementKind::Match:
		case pst::ElementKind::MatchCase:
			return ElementScopeKind::Standard;

		case pst::ElementKind::FlowPattern:
		case pst::ElementKind::BindingPattern:
		case pst::ElementKind::WildcardPattern:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::ClassConstructor:
		case pst::ElementKind::ClassDestructor:
			return ElementScopeKind::Standard;

		case pst::ElementKind::ExprStmt:
			return ElementScopeKind::Transparent;
		case pst::ElementKind::OperatorWrapper:
		case pst::ElementKind::IdentifierWrapper:
		case pst::ElementKind::KeywordWrapper:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::ExprElement:
		case pst::ElementKind::RoundGroupExpr:
		case pst::ElementKind::CallList:
		case pst::ElementKind::AtrArgList:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::ExprHolder: {
			auto expr_parent = element->getParent().value().unlockOpt(ctx);
			if (expr_parent.has_value()
			    && expr_parent.value()->getElementKind() == pst::ElementKind::Expand) {
				// This is a special case.
				// Elements in macro expansions should have their scope parent be the grandparent.
				return ElementScopeKind::ParentTransparent;
			}
			if (expr_parent.has_value()
			    && expr_parent.value()->getElementKind() == pst::ElementKind::Match) {
				// The match subject resolves in the scope surrounding the match. Resolving it
				// inside the match's own scope would create a query cycle: enumerating the
				// match scope's symbols requires compiling the subject.
				return ElementScopeKind::ParentTransparent;
			}
			return ElementScopeKind::Transparent;
		}

		case pst::ElementKind::Param:
		case pst::ElementKind::ParamList:
		case pst::ElementKind::TemplateList:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::Expand:
			// This is a bit of a special case, we treat it as transparent, since it is
			// basically just a wrapper around the expanded element.
			return ElementScopeKind::Transparent;

		case pst::ElementKind::TemplateStmt:
			// This defines a non-empty scope only for for baked PST nodes,
			// @TODO: #3071 probably change it to transparent,
			// since this element should not be present in baked PSTs (or will be a trivial node).
			return ElementScopeKind::Standard;

		case pst::ElementKind::TemplateDecl:
			// This is a weird case, this is used to lookup on expressions inside template
			// declaration before baking.
			return ElementScopeKind::Transparent;

		case pst::ElementKind::FormatSubExpression:
		case pst::ElementKind::FormatSubString:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::KindNotSet:
			CORE_UNREACHABLE();

		default:
			CORE_PANIC("PST element scope kind for: ", element->elementType());
		}
		CORE_UNREACHABLE();
	}

	struct IMPLEMENT_QUERY(QueryRootScopeOf, ScopeData) {
		static auto provide(Context&, QKey key) -> PResult {
			return ScopeData{ {}, true, {}, key, 0 };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK

	private:
		/**
		 * @brief This is a helper function for getAllHeliosScopes.
		 * Use only inside that function (and only for debug/test purposes)!
		 */
		static std::vector<ScopeID> getAllCachedScopes() {
			// This implementation is fragile, adjust if needed.

			std::vector<ScopeID> out;

			for (auto& [key, cache_entry]: cache) out.emplace_back(QResult{ &cache_entry.data });
			return out;
		}

		// for getAllCachedScopes:
		friend std::vector<ScopeID> getAllHeliosScopes();
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRootScopeOf);

	struct IMPLEMENT_QUERY(QueryPrimaryCodeScopeFor, ScopeData) {
		/**
		 * @brief Cache to verify parent scopes are consistent.
		 * @note It is intentionally thread safe.
		 */
		inline static concurrent::ConHashMap<pst::PstID, ScopeID> parent_map;

		static auto provide(Context& ctx, QKey element_key) -> PResult {
			auto element            = element_key.element.unlock(ctx);
			auto element_scope_kind = getScopeKind(ctx, element_key.element);

			if (element_scope_kind == ElementScopeKind::Invalid) {
				[[maybe_unused]] auto element_ptr = &*element;
				CORE_PANIC(base::strConcat(
					"Scope of element for which scope does not make sense (or was not "
					"added.): ",
					typeid(*element_ptr).name()
				));
			}

			ScopeID parent = [&]() -> ScopeID {
				auto pst_parent = getPSTElementParent(ctx, element);

				if (pst_parent.isLangElement())
					return ctx.query<QueryPrimaryCodeScopeFor>(pst_parent.getAsLangElement());
				else
					return ctx.query<QueryRootScopeOf>(pst_parent.getAsModuleID());
			}();


			// here we essentially return the same scope as the parent
			// scope, with the same unstable hash, but we still create a new ScopeData object
			// that is kept in our cache:
			if (element_scope_kind == ElementScopeKind::Transparent) {
				return parent.ref->perfectClone();
			} else if (element_scope_kind == ElementScopeKind::ParentTransparent) {
				CORE_ASSERT(
					parent.ref->parent.has_value(),
					"Parent transparent scope kind used on element of which transparent scope that "
					"has no parent."
				);
				return parent.ref->parent->ref->perfectClone();
			}


			// simple parent sanity check:
			// it is technically not needed anymore, but it left as an additional
			// layer of bug detection.
			parent_map.maybePutAndUpdate(element->getID(), parent, [&](CRef<ScopeID> existing) {
				CORE_ASSERT(*existing == parent, "Parent mismatch in QueryPrimaryCodeScopeFor");
			});

			return ScopeData{
				parent, false, element->getHash(), module(parent), scopeDepth(parent) + 1,
			};
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK

	private:
		/**
		 * @brief This is a helper function for getAllHeliosScopes.
		 * Use only inside that function (and only for debug/test purposes)!
		 */
		static std::vector<ScopeID> getAllCachedScopes() {
			// This implementation is fragile, adjust if needed.

			std::vector<ScopeID> out;

			for (auto& [key, cache_entry]: cache) out.emplace_back(QResult{ &cache_entry.data });
			return out;
		}

		// for getAllCachedScopes:
		friend std::vector<ScopeID> getAllHeliosScopes();
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPrimaryCodeScopeFor);

	ScopeID queryBodyCodeScopeFor(query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt) {
		// note: not all cases are handled here, which is intentional.
		// We might add more in the future, but this function should remain a simple one.

		struct QueryBodyScopeVisitor: pst::PstVisitorPanicky {
			query::Context&              ctx;
			pst::AccessLocked<pst::Stmt> stmt;

			QueryBodyScopeVisitor(query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt):
				  ctx(ctx),
				  stmt(stmt) {}

			base::Optional<ScopeID> out;

			void visitNamespace(pst::Access<pst::Namespace> namespace_stmt) override {
				out = ctx.query<QueryPrimaryCodeScopeFor>({ namespace_stmt->getBody() });
			}

			void visitClass(pst::Access<pst::Class> class_stmt) override {
				out = ctx.query<QueryPrimaryCodeScopeFor>({ class_stmt->getBody() });
			}
		};

		QueryBodyScopeVisitor visitor(ctx, stmt);
		stmt.unlock(ctx)->acceptVisitor(visitor);
		return visitor.out.value();
	}

	struct IMPLEMENT_QUERY(QueryScopesInModule, QueryScopesInModuleValue) {
		/**
		 * Helper struct used for accumulating the query output.
		 */
		struct Output final {
			std::vector<ScopeID> scopes;
			bool                 failed = false;
		};

		static auto getScopes(Context& ctx, frontend::FileID file, Ref<Output> out) {
			auto root          = getFilePST(ctx, file)->getRootElement();
			auto root_unlocked = root.unlockOpt(ctx);
			if (root_unlocked.empty()) {
				// PST root failed to parse, PST should have already reported the diagnostic.
				out->failed = true;
				return;
			}

			auto grab_scopes_function = [&ctx, &out](pst::AccessLocked<pst::LangElement> element) {
				// handling lambdas, templates, etc. might be much more tricky and
				// require a different approach
				if (getScopeKind(ctx, element) == ElementScopeKind::Standard)
					out->scopes.emplace_back(ctx.query<QueryPrimaryCodeScopeFor>(element));
			};

			// @TODO: #3080 cutoff changes semantics of this query, consider making it internal
			// somehow. Note that more custom logic might be needed in the future (to optimize it,
			// to compile lambdas, etc.)

			auto cutoff_function = [](pst::Access<pst::LangElement> element) {
				if (element->getElementKind() == pst::ElementKind::TemplateStmt) {
					// we want to skip template bodies, since
					// they don't compile directly
					return true;
				}
				return false;
			};

			auto for_all_ok
				= pstForAll(ctx, root_unlocked.value(), grab_scopes_function, cutoff_function);
			if (for_all_ok.status().isBad()) {
				// if the pstForAll failed, we mark the whole query as failed, but we still return
				// the scopes that we managed to obtain.
				out->failed = true;
			}
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			// fetch scopes from main module file
			auto main_file = ctx.query<frontend::QueryMainSourceFile>(key);

			Output output;
			output.scopes.reserve(1'024);  // there will usually be a lot of scopes

			getScopes(ctx, main_file, &output);

			// eliminate duplicates with sort:
			std::ranges::sort(output.scopes);
			auto [unique_end, unique_last] = std::ranges::unique(output.scopes);
			output.scopes.erase(unique_end, output.scopes.end());

			// validate output:
			for (auto scope: output.scopes)
				CORE_ASSERT(module(scope) == key, "Module mismatch in QueryScopesInModule\n");

			if (output.failed) {
				return QueryScopesInModuleValue{ .value = QueryScopesInModuleValue::Failure{ .partial_scopes = std::move(output.scopes), }, };
			} else {
				return QueryScopesInModuleValue{ .value = QueryScopesInModuleValue::Success{ .scopes = std::move(output.scopes), }, };
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryScopesInModule);

	struct IMPLEMENT_QUERY(QuerySymbolsInScope, query::QResult<std::vector<SymID>>) {
		/**
		 * @brief Makes symbols from pst::Stmt and filters out non declarations from the StmtList.
		 */
		template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
		static std::vector<SymID> filterSymbolsFromStmtList(
			query::Context& ctx, const StmtList<Stmt>& list
		) {
			std::vector<SymID> symbols;
			for (const auto& stmt: list) {
				switch (stmt.unlock(ctx)->isDeclaration()) {
				case pst::DeclKind::Symbol: {
					// @TODO: #1753 Maybe we should skip the symbol if compiling the symbol failed.
					auto sym_id = ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();
					symbols.emplace_back(sym_id);
					break;
				}
				case pst::DeclKind::Transparent: {
					if (auto stmt_specifier_opt
					    = stmt.unlock(ctx).template dynamicCast<pst::SpecifierBlock>()) {
						auto stmt_specifier = stmt_specifier_opt.value();
						auto inner_symbols  = filterSymbolsFromStmtList(
                            ctx, getStmtsFromStmtAggregate(ctx, stmt_specifier->getBlock())
                        );
						symbols.insert(symbols.end(), inner_symbols.begin(), inner_symbols.end());
					} else if (auto using_opt
					           = stmt.unlock(ctx).template dynamicCast<pst::Using>()) {
						// Using has DeclType::Transparent if it ends in .*
						// This is currently handled the same way as DeclType::Symbol.
						auto sym_id = ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();
						symbols.emplace_back(sym_id);
					} else if (auto import_opt
					           = stmt.unlock(ctx).template dynamicCast<pst::Import>()) {
						// Import has DeclType::Transparent as it can intrude many different
						// symbols. This is currently handled the same way as DeclType::Symbol.
						auto sym_id = ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();
						symbols.emplace_back(sym_id);
					} else {
						CORE_PANIC(
							"Not handled element ",
							stmt.unlock(ctx)->elementType(),
							" with DeclKind::Transparent."
						);
					}
					break;
				}
				case pst::DeclKind::None:
					// do nothing
					break;
				default:
					CORE_PANIC("Not handled PST element in filterSymbolsFromStmtList");
				}
			}
			return symbols;
		}

		/**
		 * @brief Gets symbols for scopes of various statements.
		 *
		 * A visit that reports the statement as erroneous / not-yet-implemented leaves `out`
		 * unset, which the caller turns into a query failure.
		 */
		struct SymbolGrabVisitor final: pst::PstVisitorPanicky {
			SymbolGrabVisitor(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<std::vector<SymID>> out;
			Context&                           ctx;
			const QKey&                        key;

			template<class... Args>
			void output(Args&&... args) {
				CORE_ASSERT(this->out.empty(), "Output already set");
				this->out.emplace(std::forward<Args>(args)...);
			}

			// here, we add only stmts, that actually have a primary scope.

			void visitFun(pst::Access<pst::Fun> fun) override {
				// Scope of "fun →()← {}"

				std::vector<SymID> out;
				for (auto params: *fun->getParams().unlock(ctx))
					out.emplace_back(ctx.query<QuerySymbolOfSTMT>(params).valueOrThrow());

				output(std::move(out));
			}

			void visitFunDecl(pst::Access<pst::FunDecl> fun_decl) override {
				std::vector<SymID> out;
				for (auto param: *fun_decl->getParams().unlock(ctx))
					out.emplace_back(ctx.query<QuerySymbolOfSTMT>(param).valueOrThrow());
				output(std::move(out));
			}

			void visitMethod(pst::Access<pst::Method> meth) override {
				// Scope of "fun →()← {}"

				std::vector<SymID> out;
				for (auto params: *meth->getParams().unlock(ctx))
					out.emplace_back(ctx.query<QuerySymbolOfSTMT>(params).valueOrThrow());

				out.emplace_back(ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= defgen::SelfParameter{ .method_symbol
				                             = ctx.query<QuerySymbolOfSTMT>(meth).valueOrThrow(),
				                             .scope = key },
				}));

				output(std::move(out));
			}

			void visitDestructor(pst::Access<pst::Destructor> dtor) override {
				// Scope of "T.destroy →()← = {}". A destructor has no parameters, only an
				// implicit `self`.
				std::vector<SymID> out;
				out.emplace_back(ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= defgen::SelfParameter{ .method_symbol
				                             = ctx.query<QuerySymbolOfSTMT>(dtor).valueOrThrow(),
				                             .scope = key },
				}));

				output(std::move(out));
			}

			void visitConstructor(pst::Access<pst::Constructor> ctor) override {
				// Scope of "T →()← = {}" / "T.name →()← = {}". User-defined constructors are
				// parsed but nothing compiles them yet, so report it instead of falling through
				// to the panicky visitor default. Leaving `out` unset fails the query.
				// @TODO: #1290 grab the parameter symbols here once constructors are supported.
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					"User-defined constructors are not yet supported",
					ctor->getStablePosition(),
					"`T(...)` and `T.name(...)` declare a constructor.\n"
				));
			}

			void visitCopyConstructor(pst::Access<pst::CopyConstructor> cctor) override {
				// Scope of "T.copy →(other)← = {}".
				std::vector<SymID> out;
				for (auto params: *cctor->getParams().unlock(ctx))
					out.emplace_back(ctx.query<QuerySymbolOfSTMT>(params).valueOrThrow());

				output(std::move(out));
			}

			void visitIf(pst::Access<pst::If>) override {
				// Scope of "if →(...)← {}"
				// @TODO: check if "If" defines any variables in its condition
				// and add them here.
				output(std::vector<SymID>{});
			}

			void visitWhile(pst::Access<pst::While>) override {
				// Scope of "while →(...)← {}"
				// @TODO: check if "While" defines any variables in its condition
				// and add them here.
				output(std::vector<SymID>{});
			}

			void visitFor(pst::Access<pst::For> for_stmt) override {
				using namespace desugaring;
				ForGeneratedSymbols symbols = getForGeneratedSymbols(ctx, for_stmt);

				std::vector<SymID> out;
				out.emplace_back(symbols.iterator);
				out.emplace_back(symbols.index);
				out.emplace_back(symbols.length);
				output(std::move(out));
			}

			void visitExprStmt(pst::Access<pst::ExprStmt>) override {
				output(std::vector<SymID>{});
			}

			void visitTemplateStmt(pst::Access<pst::TemplateStmt> template_stmt) override {
				// Scope of "template →(...)← {}"
				// This also inserts the baked template symbols into this scope.

				// @TODO: #3071 this is a hack, fix it!
				// Here two different cases are handled:
				// * for pre-bake PST template this defined no symbols, this is a scope in which the
				// expressions from template "signature" are compiled
				// * for baked PST template this defines generated symbols for the template
				// arguments, which are used in the template body.


				std::vector<SymID> out;

				if (not template_stmt->hasAdditionalRootData()) {
					// This is not a baked template, so it does not define any symbols in its scope.
					output(out);
					return;
				}

				const auto& additional_data = template_stmt->getAdditionalRootData();

				variant_match(additional_data.pst_parent) {
					variant_case(pst::AdditionalRootData::BakedTemplateParent, template_parent) {
						auto proper_data = base::anyCast<templates::TemplateBakePSTLinkedData>(
							template_parent.template_bake_data
						);
						auto postponed_data
							= proper_data.postponed_data->load(std::memory_order_acquire);

						for (const auto& param: postponed_data->template_arguments_symbols)
							out.emplace_back(param);
						out.emplace_back(postponed_data->baked_symbol);
					}
					variant_default {
						CORE_PANIC(
							"Template declaration without BakedTemplateParent, this should not "
							"happen here."
						);
					}
				}

				output(out);
			}
		};

		/**
		 * This is an actual implementation of the query.
		 * `provide` function simply calls it and validates output.
		 */
		static auto getSymbols(Context& ctx, QKey key) -> PResult {
			if (not key.ref->related_pst_element_hash.has_value()) {
				CORE_ASSERT(key.ref->is_root, "Non root scope without PST element!");
				return {};
			}
			auto base_element = key.ref->relatedPSTElement().value().unlock(ctx);

			if (base_element->isStatementAggregate()) {
				return filterSymbolsFromStmtList(ctx, getStmtsFromStmtAggregate(ctx, base_element));
			} else if (base_element->getElementKind() == pst::ElementKind::Match) {
				// A match introduces no symbols of its own; its subject is lowered straight to
				// MIR without a generated local.
				return {};
			} else if (base_element->getElementKind() == pst::ElementKind::MatchCase) {
				// The only symbol a match case may introduce is its pattern binding.
				auto match_case = base_element.dynamicCast<pst::MatchCase>().value();
				auto flow       = match_case->getPattern().unlock(ctx);

				std::vector<SymID> out;
				if (auto binding
				    = flow->getPattern().unlock(ctx).dynamicCast<pst::BindingPattern>()) {
					out.emplace_back(
						ctx.query<QuerySymbolOfSTMT>({ binding.value()->getName() }).valueOrThrow()
					);
				}
				return out;
			} else if (base_element->isStatement()) {
				// note: if this check fail, it might be that we are missing some cases
				// @TODO: #3071 maybe modify this assertion or add a new else-if branch for template
				// statements
				CORE_ASSERT(
					getScopeKind(ctx, base_element) == ElementScopeKind::Standard,
					"Bad element in QuerySymbolsInScope"
				);

				SymbolGrabVisitor symbol_grab(ctx, key);
				auto              as_stmt = base_element.dynamicCast<pst::Stmt>().value();
				as_stmt->acceptVisitor(symbol_grab);

				if (symbol_grab.out.empty()) return query::Failed();
				return std::move(symbol_grab.out.value());
			} else if (base_element->getElementKind() == pst::ElementKind::ExprHolder) {
				return std::vector<SymID>{};
			} else {
				CORE_PANIC(
					"Query symbols from scope of non-statement, non-codeblock and non-expr-holder"
				);
			}
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto output = getSymbols(ctx, key);

			// Sanity check that the output symbols have correct scope.
			if (output.hasValue()) {
				for (auto sym: output.valueOrPanic()) {
					// @TODO: #3099 generated symbols scopes are needed
					// mostly here and potentially for mangling.
					// Just removing this assertion for them is not a way to go, since this assertion
					// ensures that scopes info is consistent. We could however add some kind of
					// "QueryAdditionalScopelessSymbolsInScope". Tho this will not be trivial.

					CORE_ASSERT(
						scope(sym) == key,
						base::strConcat(
							"Scope mismatch in QuerySymbolsInScope and QuerySymbolOfSTMT\n",
							" for symbol: ",
							name(sym),
							"\n\n"
							" considered scope : ",
							key.ref->relatedPSTElement().value().unlock(ctx)->elementType(),
							", ID: ",
							key.ref->relatedPSTElement().value().unlock(ctx)->getID().asInt(),
							"\n\n",
							" scope of symbol: ",
							scope(sym).ref->relatedPSTElement().value().unlock(ctx)->elementType(),
							", ID: ",
							scope(sym).ref->relatedPSTElement().value().unlock(ctx)->getID().asInt(),
							"\n"
						)
					);
				}
				if (key.ref->is_root)
					CORE_ASSERT(
						output.valueOrPanic().empty(), "Root scope should not have any symbols."
					);
			}

			return output;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolsInScope);

	namespace {
		/**
		 * @brief A module implicitly imported (as if by `import <package>.<path>.*;`) into every
		 * module that is not part of the standard library. Re-exports are not supported yet, so the
		 * prelude is emulated by importing the original source modules directly rather than through
		 * a dedicated `prelude` module.
		 */
		struct PreludeImport final {
			base::StrID              package;
			std::vector<base::StrID> path;
		};

		/**
		 * @brief Get the list of modules to import by default (as part of the prelude).
		 */
		const std::vector<PreludeImport>& preludeImports() {
			static const std::vector<PreludeImport> imports{
				{ .package = base::StrID("core"), .path = { base::StrID("builtins") } },
				{ .package = base::StrID("core"), .path = { base::StrID("containers") } },
				{ .package = base::StrID("core"), .path = { base::StrID("prints") } },
			};
			return imports;
		}

		/**
		 * @brief True when @p module_id belongs to the standard library itself. Such modules keep
		 * their explicit imports and must not receive the implicit prelude — this also breaks the
		 * self-import cycle that `core.builtins` importing itself would otherwise create.
		 */
		bool isStandardLibraryModule(query::Context& ctx, frontend::ModuleID module_id) {
			auto package_id = frontend::getModuleRef(module_id)->getPackage().unlock(ctx).getID();
			return frontend::packages::isStandardLibraryPackage(package_id);
		}

		/**
		 * @brief Looks up @p name as if every non-stdlib module wrote `import <module>.*;` for each
		 * configured prelude module, merging the matches into a single result. Returns an empty
		 * result (never fails) when the standard library is absent (e.g. `--no-std`, or a custom
		 * std that lacks the module) or when @p module_id is itself a standard-library module.
		 */
		query::QResult<LookupResult> lookupImplicitPrelude(
			query::Context& ctx, frontend::ModuleID module_id, base::StrID name, bool with_wildcards
		) {
			LookupResult result{};
			if (isStandardLibraryModule(ctx, module_id)) return result;

			for (const auto& prelude_import: preludeImports()) {
				auto module_opt = frontend::getModuleByAbsolutePath(
					ctx, prelude_import.package, prelude_import.path
				);
				if (module_opt.empty()) continue;

				auto prelude_scope   = queryRootScopeOfMainModuleFile(ctx, module_opt.value());
				auto prelude_qresult = HInterface::ofScope(prelude_scope)
				                           .lookup(ctx, name, { .with_wildcards = with_wildcards });
				UNPACK_QRESULT_CREF(CRef<LookupResult> prelude_result = &, prelude_qresult);

				// The prelude re-exports the contents of the module, but not the modules it
				// imports itself - otherwise every `import` written in a prelude module would
				// collide with the same import written by the user.
				LookupResult exported{ .leaves       = {},
					                   .inaccessible = {},
					                   .children     = prelude_result->children };
				for (const SymID sym: prelude_result->leaves)
					if (kind(sym) != SymbolKind::Import) exported.leaves.push_back(sym);

				result.merge(exported);
			}
			return result;
		}
	}

	struct IMPLEMENT_QUERY(QueryLookupInScope, query::QResult<LookupResult>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			Ref symbol_list = &ctx.query<QuerySymbolsInScope>(key.scope)->valueOrThrow();

			LookupResult result{};

			for (const auto& sym: *symbol_list) {
				if (isIgnoredByLookup(sym)) continue;

				if (kind(sym) == SymbolKind::Using or kind(sym) == SymbolKind::Import) {
					UNPACK_QRESULT_CREF(
						CRef<LookupResult> pointed_result = &,
						ctx.query<QueryLookupInUsingImport>({ sym, key.name, key.with_wildcards })
					);
					// The correct code that works for using is commented out,
					// to make the import a.*; work correctly.
					// @TODO: #1412 fix this properly
					// if (!pointed_result->isEmpty())
					// 	result.children.push_back(pointed_result->toNode(sym));
					if (!pointed_result->isEmpty()) result.merge(*pointed_result);
				} else if (name(sym) == key.name) {
					result.leaves.push_back(sym);
				} else {
					// nothing
				}
			}

			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInScope);

	struct IMPLEMENT_QUERY(QueryLookupInScopeAndParents, query::QResult<LookupResult>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			UNPACK_QRESULT_CREF(auto result =, ctx.query<QueryLookupInScope>(key));

			if (key.scope.ref->parent.has_value()) {
				auto parent = key.scope.ref->parent.value();

				// Reverse insertion order allow for linear result concatenation instead of
				// quadratic
				UNPACK_QRESULT_CREF(
					LookupResult parent_result =,
					ctx.query<QueryLookupInScopeAndParents>({ parent, key.name, key.with_wildcards })
				);

				parent_result.merge(std::move(result));

				return parent_result;
			}

			// At root scope.
			auto current_module_id = key.scope.ref->parent_module;

			// Bring the default prelude modules into scope, as if every module wrote
			// `import <module>.*;`. A no-op under --no-std or inside the standard library.
			UNPACK_QRESULT(
				LookupResult prelude_result =,
				lookupImplicitPrelude(ctx, current_module_id, key.name, key.with_wildcards)
			);
			result.merge(std::move(prelude_result));

			// Also check whether this is a REPL module with a parent.
			const bool is_repl_module = ctx.query<frontend::QueryIsReplModule>(current_module_id);
			auto repl_parent_opt = ctx.query<frontend::QueryReplModuleParent>(current_module_id);

			CORE_DEV_LOG(
				REPL,
				"At root scope, module #",
				current_module_id.queryUnstablePerfectHash(),
				", isRepl=",
				is_repl_module,
				", hasParent=",
				repl_parent_opt.has_value(),
				"\n"
			);

			if (is_repl_module && repl_parent_opt.has_value()) {
				// Query the parent REPL module's TopLevel scope.
				auto parent_module_id      = repl_parent_opt.value();
				auto parent_toplevel_scope = queryRootScopeOfMainModuleFile(ctx, parent_module_id);

				UNPACK_QRESULT_CREF(
					LookupResult parent_result =,
					ctx.query<QueryLookupInScopeAndParents>(
						{ parent_toplevel_scope, key.name, key.with_wildcards }
					)
				);

				parent_result.merge(std::move(result));

				return parent_result;
			}

			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInScopeAndParents);

	base::Bit256 KeyOf_LookupInScope::queryUnstablePerfectHash() const {
		auto hash_1 = scope.queryUnstablePerfectHash();
		auto hash_2 = std::hash<base::StrID>()(name);

		return { hash_1, hash_2, static_cast<u64>(with_wildcards) };
	}

	ScopeID queryRootScopeOfMainModuleFile(query::Context& ctx, frontend::ModuleID module) {
		auto main_source_file = ctx.query<frontend::QueryMainSourceFile>(module);
		auto main_source_pst  = getFilePST(ctx, main_source_file);

		auto main_file_root_scope
			= ctx.query<QueryPrimaryCodeScopeFor>({ main_source_pst->getRootElement() });

		return main_file_root_scope;
	}

	void ScopeID::debugPrintScopeAndParents(std::ostream& os) const {
		auto iter_scope = *this;

		while (true) {
			os << iter_scope.queryUnstablePerfectHash() << "("
			   << (iter_scope.ref->relatedPSTElement().has_value()
			           ? iter_scope.ref->relatedPSTElement()
			                 .value()
			                 .illegalAccess()
			                 .value()
			                 ->elementType()
			           : "ROOT")
			   << ")" << " -> ";

			if (not parent(iter_scope).has_value()) break;
			iter_scope = parent(iter_scope).value();
		}
		os << "\n";
	}

	std::vector<ScopeID> getAllHeliosScopes() {
		// this implementation is fragile, adjust if needed.

		PANIC_IF_NOT_TEST();

		CORE_ASSERT(
			!query::Context::areWeInsideQuery(), "getAllHeliosScopes called from within query!"
		);

		auto root_scopes = ImplementationOf_QueryRootScopeOf::getAllCachedScopes();
		auto pst_scopes  = ImplementationOf_QueryPrimaryCodeScopeFor::getAllCachedScopes();

		std::vector<ScopeID> out;
		out.reserve(root_scopes.size() + pst_scopes.size());
		out.insert(out.end(), root_scopes.begin(), root_scopes.end());
		out.insert(out.end(), pst_scopes.begin(), pst_scopes.end());
		return out;
	}
}

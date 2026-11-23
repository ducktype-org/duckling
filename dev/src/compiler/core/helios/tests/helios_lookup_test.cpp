// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_expr.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup.hpp>
#include <helios_private/lookup/lookup_in_type_interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/anycast.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::helios;
using namespace compiler::helios::test_utils;
using compiler::tsh::AbstractType;
using query::utils::withContextDo;

/**
 * @brief Tests of the lookups performed in the interface of a type, that is of what `obj.x` and
 * `T.x` find, and of what they hide because of its visibility.
 */
class HeliosLookupTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosLookupTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testAccessModes);
		TESTER_ADD_TEST(testVisibility);
		TESTER_ADD_TEST(testInaccessibleReporting);
		TESTER_ADD_TEST(testThroughHInterface);
		TESTER_ADD_TEST(testScopeLookupPaths);
		TESTER_ADD_TEST(testResolveUsingImport);
		TESTER_ADD_TEST(testStripQualifiers);
	}

private:
	/**
	 * @brief The class types and the scopes of the fixture module, resolved once per test.
	 */
	struct Fixture final {
		AbstractType base;
		AbstractType derived;

		/**
		 * @brief Scope of the body of a method of `Base`, that is a scope that may use even the
		 * private members of `Base`.
		 */
		ScopeID inside_base;

		/**
		 * @brief Scope of the body of a method of `Derived`, that is a scope that may use the
		 * protected members of `Base` but not its private ones.
		 */
		ScopeID inside_derived;

		/**
		 * @brief Scope of the body of a method of a class unrelated to `Base`.
		 */
		ScopeID inside_unrelated;

		/**
		 * @brief Scope of the body of a free function, outside of any class.
		 */
		ScopeID outside;
	};

	static Fixture makeFixture(ScopeID root_scope) {
		auto class_type = [&](std::string_view class_name) {
			return AbstractType(query::entryPoint<compiler::tsh::QueryClassType>(
				getChain(class_name, root_scope).back()
			));
		};

		return { .base             = class_type("Base"),
			     .derived          = class_type("Derived"),
			     .inside_base      = methodBodyScope(class_type("Base"), "insidePrivate"),
			     .inside_derived   = methodBodyScope(class_type("Derived"), "insideDerived"),
			     .inside_unrelated = methodBodyScope(class_type("ClassWithMember"), "getMemberPub"),
			     .outside = getFunctionBodyScope(getChain("freeFunction", root_scope).back()) };
	}

	/**
	 * @brief The scope of the body of a method of a class, which is where a lookup written
	 * inside that class happens.
	 */
	static ScopeID methodBodyScope(AbstractType type, std::string_view method_name) {
		return base::anyCast<ScopeID>(
			query::utils::withContextCompute([&](query::Context& ctx) -> std::any {
				const auto& elements
					= type.getInterface(ctx)->getElementsWithName(base::StrID(method_name));
				CORE_ASSERT(elements.size() == 1, "Expected exactly one method of that name");

				auto method_pst = maybeSymbolPst(elements.front().getSymbol())
			                          .value()
			                          .unlock(ctx)
			                          .dynamicCast<pst::Method>()
			                          .value();
				return ctx.query<QueryPrimaryCodeScopeFor>(method_pst->getBody().unlock(ctx));
			})
		);
	}

	static std::vector<std::string> names(const LookupPath& path) {
		std::vector<std::string> result;
		result.reserve(path.symbols.size());
		for (const SymID symbol: path.symbols)
			result.emplace_back(compiler::helios::name(symbol).strView());
		return result;
	}

	static std::vector<SymID> usingsIn(query::Context& ctx, ScopeID scope) {
		std::vector<SymID> usings;
		for (const SymID symbol: ctx.query<QuerySymbolsInScope>(scope)->valueOrThrow())
			if (kind(symbol) == SymbolKind::Using) usings.push_back(symbol);
		return usings;
	}

	static CRef<LookupResult> lookupInType(
		query::Context&         ctx,
		AbstractType            type,
		std::string_view        name,
		TypeAccessMode          mode,
		base::Optional<ScopeID> accessing_scope
	) {
		return &ctx.query<QueryLookupInType>({ .type            = type,
		                                       .name            = base::StrID(name),
		                                       .mode            = mode,
		                                       .accessing_scope = accessing_scope })
		            ->valueOrThrow();
	}

	/**
	 * @brief A value of a type reaches everything it declares, its type alone reaches only what
	 * does not need an instance.
	 */
	void testAccessModes() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes")));
		const auto fixture           = makeFixture(root_scope);

		withContextDo([&](query::Context& ctx) {
			auto found = [&](std::string_view name, TypeAccessMode mode) {
				return lookupInType(ctx, fixture.base, name, mode, fixture.inside_base)
				    ->leaves()
				    .size();
			};

			ASSERT_EQUAL(1u, found("pub_field", TypeAccessMode::Instance));
			ASSERT_EQUAL(1u, found("getPub", TypeAccessMode::Instance));
			ASSERT_EQUAL(1u, found("counter", TypeAccessMode::Instance));
			ASSERT_EQUAL(1u, found("make", TypeAccessMode::Instance));

			// A field and a method need a value to be used on, so the type alone cannot reach
			// them.
			ASSERT_EQUAL(0u, found("pub_field", TypeAccessMode::Meta));
			ASSERT_EQUAL(0u, found("getPub", TypeAccessMode::Meta));
			ASSERT_EQUAL(1u, found("counter", TypeAccessMode::Meta));
			ASSERT_EQUAL(1u, found("make", TypeAccessMode::Meta));

			// A name that the type does not declare at all is found in neither mode.
			ASSERT_EQUAL(0u, found("nonExistent", TypeAccessMode::Instance));
			ASSERT_EQUAL(0u, found("nonExistent", TypeAccessMode::Meta));
		});
	}

	/**
	 * @brief What each scope may reach, decided by the class it is written in.
	 */
	void testVisibility() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes")));
		const auto fixture           = makeFixture(root_scope);

		withContextDo([&](query::Context& ctx) {
			auto visible = [&](std::string_view name, base::Optional<ScopeID> scope) {
				return not lookupInType(ctx, fixture.base, name, TypeAccessMode::Instance, scope)
				               ->leaves()
				               .empty();
			};

			// A public member is reachable from everywhere, even from a lookup that comes from
			// no scope at all.
			ASSERT_TRUE(visible("pub_field", fixture.inside_base));
			ASSERT_TRUE(visible("pub_field", fixture.inside_derived));
			ASSERT_TRUE(visible("pub_field", fixture.inside_unrelated));
			ASSERT_TRUE(visible("pub_field", fixture.outside));
			ASSERT_TRUE(visible("pub_field", {}));

			// A protected member is reachable from the class that declares it and from the
			// classes that inherit from it.
			ASSERT_TRUE(visible("prot_field", fixture.inside_base));
			ASSERT_TRUE(visible("prot_field", fixture.inside_derived));
			ASSERT_TRUE(not visible("prot_field", fixture.inside_unrelated));
			ASSERT_TRUE(not visible("prot_field", fixture.outside));
			ASSERT_TRUE(not visible("prot_field", {}));

			// A private member is reachable only from the class that declares it.
			ASSERT_TRUE(visible("priv_field", fixture.inside_base));
			ASSERT_TRUE(not visible("priv_field", fixture.inside_derived));
			ASSERT_TRUE(not visible("priv_field", fixture.inside_unrelated));
			ASSERT_TRUE(not visible("priv_field", fixture.outside));
			ASSERT_TRUE(not visible("priv_field", {}));
		});
	}

	/**
	 * @brief A member hidden by its visibility is reported instead of being dropped, so that the
	 * error can say that it exists.
	 */
	void testInaccessibleReporting() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes")));
		const auto fixture           = makeFixture(root_scope);

		withContextDo([&](query::Context& ctx) {
			CRef hidden = lookupInType(
				ctx, fixture.base, "priv_field", TypeAccessMode::Instance, fixture.outside
			);

			ASSERT_TRUE(hidden->isEmpty());
			ASSERT_EQUAL(1u, hidden->inaccessible.size());
			ASSERT_TRUE(hidden->hasInaccessible());
			ASSERT_EQUAL(
				"priv_field", compiler::helios::name(hidden->inaccessible.front()).strView()
			);

			CRef missing = lookupInType(
				ctx, fixture.base, "nonExistent", TypeAccessMode::Instance, fixture.outside
			);
			ASSERT_TRUE(not missing->hasInaccessible());
			ASSERT_TRUE(missing->isEmpty());

			// A visible member is returned as usual, nothing is hidden along with it.
			CRef visible = lookupInType(
				ctx, fixture.base, "pub_field", TypeAccessMode::Instance, fixture.outside
			);
			ASSERT_TRUE(not visible->hasInaccessible());
			ASSERT_TRUE(visible->isSingle());
		});
	}

	/**
	 * @brief The same verdicts reached through `HInterface`, which is how the rest of the
	 * compiler performs these lookups.
	 */
	void testThroughHInterface() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes")));
		const auto fixture           = makeFixture(root_scope);

		withContextDo([&](query::Context& ctx) {
			auto instance_lookup
				= [&](std::string_view name, ScopeID scope) -> const LookupResult& {
				return HInterface::ofTypeInstance(fixture.base)
				    .lookup(ctx, base::StrID(name), { .accessing_scope = scope })
				    ->valueOrThrow();
			};
			auto meta_lookup = [&](std::string_view name, ScopeID scope) -> const LookupResult& {
				return HInterface::ofTypeMeta(fixture.base)
				    .lookup(ctx, base::StrID(name), { .accessing_scope = scope })
				    ->valueOrThrow();
			};

			ASSERT_TRUE(instance_lookup("priv_field", fixture.inside_base).isSingle());
			ASSERT_TRUE(instance_lookup("priv_field", fixture.outside).hasInaccessible());

			// The meta lookup reaches the static members only.
			ASSERT_TRUE(meta_lookup("counter", fixture.outside).isSingle());
			ASSERT_TRUE(meta_lookup("pub_field", fixture.outside).isEmpty());
		});
	}

	/**
	 * @brief A symbol reached in several ways giving the same access is found once, the paths
	 * keep the `using` the symbol was found through, and a path through a value constructs an
	 * access expression.
	 */
	void testScopeLookupPaths() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/lookup_paths")));
		const SymID values           = getChain("Values", root_scope).back();
		const SymID inner            = getChain("Values.Inner", root_scope).back();
		const SymID aliased          = getChain("Aliased", root_scope).back();
		const SymID other            = getChain("TwoValues.Other", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			auto lookup = [&](ScopeID scope, std::string_view name) -> const LookupResult& {
				return HInterface::ofScopeWithParents(scope)
				    .lookup(ctx, base::StrID(name))
				    ->valueOrThrow();
			};
			auto in_namespace = [&](SymID symbol, std::string_view name) -> const LookupResult& {
				return lookup(scopeOfNamespaceLike(ctx, symbol), name);
			};

			const auto& c = lookup(root_scope, "c");
			ASSERT_TRUE(c.isSingle());
			ASSERT_TRUE(not c.paths.front().constructsAccessExpr());

			const auto& only_wild = lookup(root_scope, "only_wild");
			ASSERT_TRUE(only_wild.isSingle());
			ASSERT_EQUAL(2u, only_wild.paths.front().symbols.size());
			ASSERT_TRUE(kind(only_wild.paths.front().symbols.front()) == SymbolKind::Using);
			ASSERT_TRUE(not only_wild.paths.front().constructsAccessExpr());

			const auto& x = in_namespace(values, "x");
			ASSERT_TRUE(x.isSingle());
			ASSERT_EQUAL((std::vector<std::string>{ "v", "x" }), names(x.paths.front()));
			ASSERT_TRUE(x.paths.front().constructsAccessExpr());

			ASSERT_EQUAL(
				(std::vector<std::string>{ "sf" }), names(in_namespace(values, "sf").paths.at(0))
			);
			ASSERT_TRUE(in_namespace(values, "sf").isSingle());
			ASSERT_TRUE(in_namespace(values, "sm").isSingle());
			ASSERT_TRUE(in_namespace(inner, "x").isSingle());
			ASSERT_TRUE(in_namespace(aliased, "x").isSingle());
			ASSERT_EQUAL(2u, in_namespace(other, "x").paths.size());
		});
	}

	/**
	 * @brief A `using` resolved without the whole chains keeps only what takes part in the
	 * access to its target.
	 */
	void testResolveUsingImport() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/lookup_paths")));
		const SymID n_namespace      = getChain("N", root_scope).back();
		const SymID qualified        = getChain("Qualified", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			auto resolve = [&](SymID symbol, bool keep_whole_chains) -> const UsingImportTarget& {
				return ctx
				    .query<QueryResolveUsingImport>({ .symbol            = symbol,
				                                      .keep_whole_chains = keep_whole_chains })
				    ->valueOrThrow();
			};

			const auto root_usings = usingsIn(ctx, root_scope);
			ASSERT_EQUAL(2u, root_usings.size());

			const auto& wildcard = std::get<TargetScope>(resolve(root_usings.at(0), false));
			ASSERT_TRUE(wildcard.chain.symbols.empty());
			ASSERT_EQUAL(scopeOfNamespaceLike(ctx, n_namespace), wildcard.scope);
			ASSERT_EQUAL(
				(std::vector<std::string>{ "N" }),
				names(std::get<TargetScope>(resolve(root_usings.at(0), true)).chain)
			);

			ASSERT_EQUAL(
				(std::vector<std::string>{ "c" }),
				names(std::get<TargetExpr>(resolve(root_usings.at(1), false)).chain)
			);
			ASSERT_EQUAL(
				(std::vector<std::string>{ "N", "c" }),
				names(std::get<TargetExpr>(resolve(root_usings.at(1), true)).chain)
			);

			const SymID value_wildcard = usingsIn(ctx, scopeOfNamespaceLike(ctx, qualified)).at(0);
			ASSERT_EQUAL(
				(std::vector<std::string>{ "hv" }),
				names(std::get<WildcardExpr>(resolve(value_wildcard, false)).chain)
			);
			ASSERT_EQUAL(
				(std::vector<std::string>{ "Holder", "hv" }),
				names(std::get<WildcardExpr>(resolve(value_wildcard, true)).chain)
			);
		});
	}

	/**
	 * @brief Only the symbols needed to access the leaf of a path are kept, and the `using`
	 * symbols are kept on request.
	 */
	void testStripQualifiers() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/lookup_paths")));
		const SymID n_namespace      = getChain("N", root_scope).back();
		const SymID c                = getChain("N.c", root_scope).back();
		const SymID holder           = getChain("Holder", root_scope).back();
		const SymID hv               = getChain("Holder.hv", root_scope).back();
		const SymID x                = getChain("v.x", root_scope).back();
		const SymID v                = getChain("v", root_scope).back();
		const SymID s                = getChain("S", root_scope).back();
		const SymID sf               = getChain("S.sf", root_scope).back();
		const SymID sm               = getChain("S.sm", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			const SymID using_symbol = usingsIn(ctx, root_scope).at(0);
			auto        strip        = [&](std::vector<SymID> symbols, bool keep_using_imports) {
                return names(stripQualifiers(
                    ctx, LookupPath{ .symbols = std::move(symbols) }, keep_using_imports
                ));
			};
			using Names = std::vector<std::string>;

			ASSERT_EQUAL((Names{ "c" }), strip({ n_namespace, c }, true));
			ASSERT_EQUAL((Names{ "hv", "x" }), strip({ holder, hv, x }, true));
			ASSERT_EQUAL((Names{ "sf" }), strip({ v, sf }, true));
			ASSERT_EQUAL((Names{ "sm" }), strip({ s, sm }, true));
			ASSERT_EQUAL((Names{ "x" }), strip({ x }, true));
			ASSERT_EQUAL(
				(Names{ compiler::helios::name(using_symbol).str(), "c" }),
				strip({ using_symbol, n_namespace, c }, true)
			);
			ASSERT_EQUAL((Names{ "c" }), strip({ using_symbol, n_namespace, c }, false));

			ASSERT_TRUE((LookupPath{ .symbols = { hv, x } }.constructsAccessExpr()));
			ASSERT_TRUE((not LookupPath{ .symbols = { using_symbol, c } }.constructsAccessExpr()));
		});
	}

public:
	~HeliosLookupTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")

#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_in_type_interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

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
				    ->leaves.size();
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
				               ->leaves.empty();
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

			ASSERT_TRUE(hidden->leaves.empty());
			ASSERT_EQUAL(1u, hidden->inaccessible.size());
			ASSERT_TRUE(hidden->hasInaccessible());
			ASSERT_EQUAL(
				"priv_field", compiler::helios::name(hidden->inaccessible.front()).strView()
			);

			// Every candidate being hidden is an error of its own, telling the name apart from
			// one that does not exist.
			ASSERT_TRUE(
				std::holds_alternative<errors::Inaccessible>(hidden->getAsSingle().valueOrThrow())
			);

			CRef missing = lookupInType(
				ctx, fixture.base, "nonExistent", TypeAccessMode::Instance, fixture.outside
			);
			ASSERT_TRUE(not missing->hasInaccessible());
			ASSERT_TRUE(std::holds_alternative<errors::SymbolNotFound>(
				missing->getAsSingle().valueOrThrow()
			));

			// A visible member is returned as usual, nothing is hidden along with it.
			CRef visible = lookupInType(
				ctx, fixture.base, "pub_field", TypeAccessMode::Instance, fixture.outside
			);
			ASSERT_TRUE(not visible->hasInaccessible());
			ASSERT_TRUE(visible->isSingle());
			ASSERT_TRUE(std::holds_alternative<SymbolList>(visible->getAsSingle().valueOrThrow()));
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
			auto instance_lookup = [&](std::string_view name, ScopeID scope) {
				return HInterface::ofTypeInstance(fixture.base)
				    .lookup(ctx, base::StrID(name), { .accessing_scope = scope })
				    ->valueOrThrow();
			};
			auto meta_lookup = [&](std::string_view name, ScopeID scope) {
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

public:
	~HeliosLookupTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")

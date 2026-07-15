/**
 * @file helios_with_std_test.cpp
 * @brief HELIOS tests that need the standard library available (e.g. to resolve
 * `import core.builtins.*`). They initialize the compiler via the driver test utils, unlike
 * helios_test.cpp which builds standalone module trees without a std.
 */

#include <driver/test_utils.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/mutability.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <any>

using namespace compiler::helios::test_utils;

class HeliosWithStdTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosWithStdTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBuiltinDefinitionInModuleHOUT);
		TESTER_ADD_TEST(testStrings);
		TESTER_ADD_TEST(testStringClassProperties);
		TESTER_ADD_TEST(testDefaultInitializers);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("test_modules/builtins")), "builtins" },
			{ fs::FilePath(path("test_modules/strings")), "strings" }
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;
	using enum compiler::tsh::Mutability;

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getIntegralTypeNoContext(
		u64 size, compiler::tsh::IntegralAbstractType::Signedness signedness
	) {
		return std::any_cast<compiler::tsh::IntegralAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return compiler::tsh::getIntegralType(ctx, size, signedness);
			})
		);
	}

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getStringTypeNoContext() {
		return std::any_cast<compiler::tsh::ClassAbstractType>(query::utils::withContextCompute(
			[&](query::Context& ctx) { return compiler::tsh::getStringType(ctx); }
		));
	}

	/**
	 * Shorthand to create a mutable symbol type from an abstract type.
	 */
	static compiler::tsh::SymbolType<> st(const compiler::tsh::AbstractType abstract_type) {
		return compiler::tsh::SymbolType{
			abstract_type,
			compiler::tsh::ReferenceKind::Direct,
			compiler::tsh::Mutability::Mutable,
		};
	}

	static compiler::tsh::SymbolType<> stConst(const compiler::tsh::AbstractType abstract_type) {
		return st(abstract_type).withMutability(compiler::tsh::Mutability::Immutable);
	}

	auto getSliceTypeNoContext(compiler::tsh::SymbolType<> element_type) {
		return std::any_cast<compiler::tsh::SliceAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return ctx.query<compiler::tsh::QuerySliceType>(element_type);
			})
		);
	}

	
	void testDefaultInitializers() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/default_constructors")));

		auto trivial_sym      = getChain("Trivial", root_scope).back();
		auto with_init_sym    = getChain("WithInit", root_scope).back();
		auto nested_sym       = getChain("Nested", root_scope).back();
		auto arr_holder_sym   = getChain("ArrayHolder", root_scope).back();
		auto tup_holder_sym   = getChain("TupleHolder", root_scope).back();
		auto deep_sym         = getChain("DeepStack", root_scope).back();
		auto deep_trivial_sym = getChain("DeepStackTrivial", root_scope).back();
		auto tup_trivial_sym  = getChain("TupleTrivial", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type = [&](SymID sym_id) {
				return ctx.query<QueryTypeFromDefinition>(sym_id)->valueOrThrow();
			};

			auto i32_st = st(compiler::tsh::getIntegralType(
				ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
			));

			// Primitives should be zero initialized.
			{
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(i32_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}

			// Unit type should be initialized with a unit literal.
			{
				auto        unit_st = st(compiler::tsh::getUnitType());
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(unit_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const LiteralUnitExpr*>(expr.get()) != nullptr);
			}

			// Meta type should be initialized with a type literal (void by default).
			{
				auto        meta_st = st(compiler::tsh::getMetaType());
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(meta_st)->valueOrThrow();
				auto        type_lit = dynamic_cast<const LiteralTypeExpr*>(expr.get());
				ASSERT_TRUE(type_lit != nullptr);
				ASSERT_EQUAL(compiler::tsh::getVoidType(), type_lit->value_type.getType());
			}

			// Trivial class should be zero initialized.
			{
				auto        trivial_st = get_class_type(trivial_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(trivial_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}

			// Class with an initial value provided for field should emit a call to a ctor.
			{
				auto        with_init_st = get_class_type(with_init_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(with_init_st)->valueOrThrow();

				auto call = dynamic_cast<const CallExpr*>(expr.get());
				ASSERT_TRUE(call != nullptr);

				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps
					= ctx.query<QueryTransitiveUsedSymbols>(ctor_sym)->valueOrThrow().used_functions;

				// The ctor only assigns a literal to the field, so it transitively uses no
				// functions (the root ctor itself is excluded from the result).
				ASSERT_EQUAL_PRINT(0, deps.size());
			}

			// Class with a class field which is non zero-initializable should emit a ctor call.
			// This ctor should call a ctor of the inner non zero-initializable field.
			{
				auto        nested_st = get_class_type(nested_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(nested_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps
					= ctx.query<QueryTransitiveUsedSymbols>(ctor_sym)->valueOrThrow().used_functions;

				// The top-level constructor (excluded from the result) transitively uses one
				// function: a default ctor of `WithInit`.
				ASSERT_EQUAL_PRINT(1, deps.size());

				const auto* dep_ctor = std::get_if<Constructor>(&getSymRef(deps[0])->other);
				ASSERT_TRUE(dep_ctor != nullptr);
				ASSERT_EQUAL(dep_ctor->kind, Constructor::Kind::Default);
				ASSERT_EQUAL(dep_ctor->type.getKind(), compiler::tsh::Kind::Class);
			}

			// ArrayHolder ctor should call a ctor of static array field, which calls a ctor of the
			// inner element.
			{
				auto        arr_holder_st = get_class_type(arr_holder_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(arr_holder_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps
					= ctx.query<QueryTransitiveUsedSymbols>(ctor_sym)->valueOrThrow().used_functions;

				// Ctor(ArrayHolder) (the root, excluded) -> Ctor(WithInit[5]) (`__init_array`) ->
				// Ctor(WithInit), plus the bounds-check chain emitted by the static-array init loop
				// (`panic`, `builtin_output_str`, `length`) and its own transitive callees: `abort`
				// from `panic`, and the string-printing chain of `builtin_output_str`
				// (`writeStr` -> `writeChar` overloads -> `putchar`) together with
				// `builtin_output_char`, which the MIR-level used-symbol collection sees.
				ASSERT_EQUAL_PRINT(12, deps.size());

				bool found_array_ctor = false;
				for (auto d: deps) {
					const auto* ctor = std::get_if<Constructor>(&getSymRef(d)->other);
					if (ctor != nullptr && ctor->kind == Constructor::Kind::Default
					    && ctor->type.getKind() == compiler::tsh::Kind::StaticArray)
						found_array_ctor = true;
				}
				ASSERT_TRUE(found_array_ctor);
			}

			// TupleHolder ctor should call a ctor of the tuple field, which calls a ctor of the
			// inner element.
			{
				auto        tup_holder_st = get_class_type(tup_holder_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(tup_holder_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps
					= ctx.query<QueryTransitiveUsedSymbols>(ctor_sym)->valueOrThrow().used_functions;

				// Ctor(TupleHolder) (the root, excluded) -> Ctor((WithInit, WithInit)) ->
				// Ctor(WithInit)
				ASSERT_EQUAL_PRINT(2, deps.size());

				bool found_tup_ctor = false;
				for (auto d: deps) {
					const auto* ctor = std::get_if<Constructor>(&getSymRef(d)->other);
					if (ctor != nullptr && ctor->kind == Constructor::Kind::Default
					    && ctor->type.getKind() == compiler::tsh::Kind::Tuple)
						found_tup_ctor = true;
				}
				ASSERT_TRUE(found_tup_ctor);
			}

			// `DeepStack` ctor should call a ctor of the `Nested` field, which calls a ctor of
			// `WithInit`
			{
				auto        deep_st = get_class_type(deep_sym);
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(deep_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps
					= ctx.query<QueryTransitiveUsedSymbols>(ctor_sym)->valueOrThrow().used_functions;

				// Ctor(DeepStack) (the root, excluded) -> Ctor(Nested) -> Ctor(WithInit)
				ASSERT_EQUAL_PRINT(2, deps.size());
			}

			// `DeepStackTrivial` ctor should not call any default constructors, since it stores
			// a static array of trivially zero-initializable types which can be zero initialized,
			// thus its zero-initializable.
			{
				auto        deep_trivial_st = get_class_type(deep_trivial_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(deep_trivial_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}

			// `TupleTrivial` ctor should not call any default constructors, since it stores
			// a tuple of trivially zero-initializable types which can be zero initialized,
			// thus its zero-initializable.
			{
				auto        tup_trivial_st = get_class_type(tup_trivial_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(tup_trivial_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}
		});
	}

	// A `@builtin(...)` fundecl (here `char_ptr_from_slice` from core.builtins) has no body in
	// source; the compiler synthesizes its implementation (getBuiltinImpl). This checks that the
	// synthesized definition is actually emitted into the module HOUT when the builtin is used.
	void testBuiltinDefinitionInModuleHOUT() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("builtins");

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module_id)
		                 .valueOrPanic();

		bool found_char_ptr_from_slice = false;
		for (const auto& hout: houts)
			for (const auto& fun: hout->functions)
				if (fun->declaration->original_name == base::StrID("char_ptr_from_slice"))
					found_char_ptr_from_slice = true;

		assertTrue(
			found_char_ptr_from_slice,
			"char_ptr_from_slice builtin definition should be emitted into the module HOUT"
		);
	}

	// `String` is now an ordinary standard-library class (no longer a special compiler type), so
	// its type properties are only observable with the std available. This checks the properties
	// the old (removed) `tsh` `simpleString` test used to assert against the compiler builtin.
	void testStringClassProperties() {
		const auto string_type = getStringTypeNoContext();

		assertTrue(
			string_type.getKind() == compiler::tsh::Kind::Class,
			"String should now be an ordinary class type."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			assertFalse(
				string_type.hasNoOpDestructor(ctx),
				"String should not have a no-op destructor: it defines one to free its buffer."
			);

			const auto string_st = st(string_type);

			assertTrue(
				string_st.isDefaultConstructible(ctx), "String should be default constructible."
			);
			assertTrue(
				string_st.isTriviallyZeroInitializable(ctx),
				"String should be trivially zero-initializable (the empty string is all-zero)."
			);
			assertTrue(string_st.isCopyable(ctx), "String should be copyable.");
			assertFalse(
				string_st.isTriviallyCopyable(ctx),
				"String should not be trivially copyable: it has a user-defined `copy`."
			);
		});
	}

	void testStrings() {
		auto  module = compiler::driver::test_utils::getModuleIdFromPath("strings");
		auto  scope  = getModuleScope(module);
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function   = hout.functions.at(0);
		auto  fun_sym    = getChain("main", scope).back();
		auto& statements = function->body->statements;
		using namespace compiler::helios::code;

		{
			// let ab = 'a' +: (&b);
			const auto& prepended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(1));
			const auto  prepended_expr
				= dynamic_cast<const CallExpr*>(prepended_stmt.initial_value.get());
			const auto prepended_callee
				= dynamic_cast<IdentifierExpr*>(prepended_expr->callee.get());
			assertEqual(
				compiler::helios::name(prepended_callee->symbol),
				base::StrID("+:"),
				"The prepended expression should call the `+:` operator"
			);
		}

		{
			// let bcd = (&b) :+ 'c' :+ 'd';
			const auto& appended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(2));
			const auto  appended_expr
				= dynamic_cast<const CallExpr*>(appended_stmt.initial_value.get());
			const auto appended_callee = dynamic_cast<IdentifierExpr*>(appended_expr->callee.get());
			assertEqual(
				compiler::helios::name(appended_callee->symbol),
				base::StrID(":+"),
				"The appended expression should call the `:+` operator"
			);
		}

		{
			const auto& concatenated_stmt = dynamic_cast<const VariableStmt&>(*statements.at(5));
			const auto  concatenated_expr
				= dynamic_cast<const CallExpr*>(concatenated_stmt.initial_value.get());
			const auto concatenated_callee
				= dynamic_cast<IdentifierExpr*>(concatenated_expr->callee.get());
			assertEqual(
				compiler::helios::name(concatenated_callee->symbol),
				base::StrID("++"),
				"The concatenated expression should call the `++` operator"
			);
		}

		{
			// let x = 1;
			// let y = 2;
			// let format = f"Did you know that {x} plus {y} equals ({x + y})?";
			const auto& format_stmt = dynamic_cast<const VariableStmt&>(*statements.at(8));
			const auto format_expr = dynamic_cast<const CallExpr*>(format_stmt.initial_value.get());
			const auto format_callee = dynamic_cast<IdentifierExpr*>(format_expr->callee.get());
			assertEqual(
				compiler::helios::name(format_callee->symbol),
				base::StrID("toString"),
				"The format string expression should call toString"
			);
		}

		{
			const auto char_type       = compiler::tsh::getCharType();
			const auto str_type        = getStringTypeNoContext();
			const auto u64_type        = getIntegralTypeNoContext(64, Unsigned);
			const auto char_slice_type = getSliceTypeNoContext(st(char_type));

			auto fun_body_scope = getFunctionBodyScope(fun_sym);

			// Vars
			ASSERT_EQUAL(char_slice_type, getTypeOf("should_char_slice", fun_body_scope));
			ASSERT_EQUAL(char_type, getTypeOf("should_char", fun_body_scope));
			ASSERT_EQUAL(u64_type, getTypeOf("should_u64", fun_body_scope));
			ASSERT_EQUAL(str_type, getTypeOf("should_string", fun_body_scope));
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")

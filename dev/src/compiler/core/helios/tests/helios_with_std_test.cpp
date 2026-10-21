// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file helios_with_std_test.cpp
 * @brief HELIOS tests that need the standard library available (e.g. to resolve
 * `import core.builtins.*`). They initialize the compiler via the driver test utils, unlike
 * helios_test.cpp which builds standalone module trees without a std.
 */

#include <ctv/ctv.hpp>
#include <driver/test_utils.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/mutability.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios/utils/hout_walkers.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/defer.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <algorithm>
#include <any>
#include <array>
#include <iostream>
#include <regex>
#include <sstream>

using namespace compiler::helios::test_utils;

class HeliosWithStdTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosWithStdTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testImplicitPrelude);
		TESTER_ADD_TEST(testBuiltinDefinitionInModuleHOUT);
		TESTER_ADD_TEST(testTemplatedBuiltinDefinitionsInModuleHOUT);
		TESTER_ADD_TEST(testStrings);
		TESTER_ADD_TEST(testStringClassProperties);
		TESTER_ADD_TEST(testDefaultInitializers);
		TESTER_ADD_TEST(testCompTimeStrings);
		TESTER_ADD_TEST(testConstants);
		TESTER_ADD_TEST(testCompTimeOutput);
		TESTER_ADD_TEST(testHoutElementsOrigin);
		TESTER_ADD_TEST(testPointers);
		TESTER_ADD_TEST(testCopy);
		TESTER_ADD_TEST(testHoutWalkers);
		TESTER_ADD_TEST(testCopyMoveOperators);
		TESTER_ADD_TEST(testBoxes);
		TESTER_ADD_TEST(testReferenceKindCollapsing);
		TESTER_ADD_TEST(testCopyConstructors);
		TESTER_ADD_TEST(testDestructors);
		TESTER_ADD_TEST(testOperatorsWithPrimitives);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("test_modules/builtins")), "builtins" },
			{ fs::FilePath(path("test_modules/strings")), "strings" },
			{ fs::FilePath(path("test_modules/comp_time_strings")), "comp_time_strings" },
			{ fs::FilePath(path("test_modules/constants")), "constants" },
			{ fs::FilePath(path("test_modules/comp_time_output")), "comp_time_output" }
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

	static const compiler::helios::code::Expr* stripImplicitMove(
		const compiler::helios::code::Expr* expr
	) {
		const auto* move = dynamic_cast<const compiler::helios::code::MoveExpr*>(expr);
		if (move == nullptr || move->kind != compiler::helios::code::MoveExpr::MoveKind::Implicit)
			return expr;
		return move->inner.get();
	}

	/**
	 * Get the value boxed by the `boxAlloc` call or nullptr on error.
	 *
	 * `new v` becomes `boxAlloc[T](v) as box T`: the primitive of `core.containers` hands the
	 * storage back as a `ptr T`, and the cast is what turns it into the box.
	 */
	static const compiler::helios::code::Expr* boxAllocArg(const compiler::helios::code::Expr* expr
	) {
		using namespace compiler::helios;
		const auto* cast = dynamic_cast<const code::CastExpr*>(stripImplicitMove(expr));
		if (cast == nullptr) return nullptr;
		if (cast->target_type.getRefKind() != compiler::tsh::ReferenceKind::Box) return nullptr;

		const auto* call = dynamic_cast<const code::CallExpr*>(cast->source_expr.get());
		if (call == nullptr) return nullptr;
		const auto callee = getIdentifierExprSymID(call->callee.ref());
		if (!callee.has_value()) return nullptr;
		if (compiler::helios::name(callee.value()) != base::StrID("boxAlloc")) return nullptr;
		return stripImplicitMove(call->arguments.at(0).get());
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
				return compiler::tsh::SymbolType<>::withDefaults(
					ctx.query<compiler::tsh::QueryClassType>(sym_id)
				);
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

				ASSERT_EQUAL_PRINT(2, deps.size());

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

	// The implicit prelude brings `core.builtins` symbols into scope of every non-stdlib module
	// without an explicit import. The `builtins` module has no import statements at all, yet an
	// unqualified `builtin_output_i64` must resolve — that resolution is the prelude at work.
	void testImplicitPrelude() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("builtins");
		auto scope     = getModuleScope(module_id);

		auto result = query::entryPoint<compiler::helios::QueryLookupInScopeAndParents>(
			{ scope, base::StrID("builtin_output_i64"), true }
		);
		assertFalse(
			result->valueOrThrow().isEmpty(),
			"builtin_output_i64 should resolve via the implicit prelude with no explicit import"
		);
	}

	// A `@builtin(...)` fundecl (here `ptr_from_slice` from core.builtins) has no body in
	// source; the compiler synthesizes its implementation (getBuiltinImpl). This checks that the
	// synthesized definition is actually emitted into the module HOUT when the builtin is used.
	void testBuiltinDefinitionInModuleHOUT() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("builtins");

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module_id)
		                 .valueOrPanic();

		bool found_ptr_from_slice = false;
		for (const auto& hout: houts)
			for (const auto& fun: hout->functions)
				if (fun->declaration->original_name == base::StrID("ptr_from_slice"))
					found_ptr_from_slice = true;

		assertTrue(
			found_ptr_from_slice,
			"ptr_from_slice builtin definition should be emitted into the module HOUT"
		);
	}

	// The `@builtin(...)` fundecls in core.builtins are templated, so their synthesized
	// implementations are emitted once per element type they are baked for. The `builtins` module
	// runs the alloc -> slice_from_ptr_len -> ptr_from_slice -> free chain for `str` and `i32`
	// (and uses `ptr_from_slice[char]` on a string literal).
	void testTemplatedBuiltinDefinitionsInModuleHOUT() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("builtins");

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module_id)
		                 .valueOrPanic();

		// Element types of the slices the baked builtins operate on, per builtin name.
		std::map<base::StrID, std::vector<compiler::tsh::AbstractType>> baked_element_types;

		for (const auto& hout: houts)
			for (const auto& fun: hout->functions) {
				const auto name        = fun->declaration->original_name;
				const bool takes_slice = name == base::StrID("ptr_from_slice");
				if (!takes_slice && name != base::StrID("slice_from_ptr_len")) continue;

				// `ptr_from_slice` takes the slice, `slice_from_ptr_len` returns it.
				const auto slice_st   = takes_slice ? fun->declaration->parameters.at(0).type
				                                    : fun->declaration->return_type;
				const auto slice_type = slice_st.getType().as<compiler::tsh::SliceAbstractType>();
				baked_element_types[name].push_back(slice_type.getElementType().getType());
			}

		query::utils::withContextDo([&](query::Context& ctx) {
			const compiler::tsh::AbstractType str_type = compiler::tsh::getCharSliceType(ctx);
			const auto                        i32_type = compiler::tsh::getIntegralType(
                ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
            );

			for (const auto* name: { "ptr_from_slice", "slice_from_ptr_len" }) {
				const auto& element_types = baked_element_types[base::StrID(name)];

				assertTrue(
					std::ranges::find(element_types, str_type) != element_types.end(),
					base::strConcat(name, " should be baked for `str`")
				);
				assertTrue(
					std::ranges::find(element_types, i32_type) != element_types.end(),
					base::strConcat(name, " should be baked for `i32`")
				);
			}
		});
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
				string_type.isTriviallyDestructible(ctx),
				"String should not be trivially destructible: it defines one to free its buffer."
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

	void testCompTimeStrings() {
		auto module     = compiler::driver::test_utils::getModuleIdFromPath("comp_time_strings");
		auto root_scope = getModuleScope(module);

		// `const a = getString();` should materialize a `String` CTV holding the source string.
		auto a_value
			= getConstValueAs<compiler::ctv::CompileTimeValue::StringClassValue>("a", root_scope);
		ASSERT_EQUAL(base::StrID("fun fromString() -> i64 = 1;"), a_value.value);

		// `const b = getCharSlice();` should materialize a char-slice CTV (stored as a `StrID`).
		auto b_value
			= getConstValueAs<compiler::ctv::CompileTimeValue::CharSliceValue>("b", root_scope);
		ASSERT_EQUAL(base::StrID("fun fromSlice() -> i64 = 2;"), b_value.value);

		// The `expand`s above should have injected `fromString`/`fromSlice` into `expanded`.
		ASSERT_TRUE(!getChain("expanded.fromString", root_scope).empty());
		ASSERT_TRUE(!getChain("expanded.fromSlice", root_scope).empty());
	}

	void testConstants() {
		auto module     = compiler::driver::test_utils::getModuleIdFromPath("constants");
		auto root_scope = getModuleScope(module);

		ASSERT_EQUAL(1'107, getConstValueAs<i64>("M", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i32>("N.X", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i32>("A", root_scope));
		ASSERT_EQUAL(-3, getConstValueAs<i32>("B", root_scope));
		ASSERT_EQUAL(-1, getConstValueAs<i64>("D", root_scope));
		ASSERT_EQUAL(6, getConstValueAs<i32>("E", root_scope));
		ASSERT_EQUAL(27, getConstValueAs<i32>("MOD", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("H2", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("T0", root_scope));
		ASSERT_EQUAL(2, getConstValueAs<i64>("T1", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("T2", root_scope));
		ASSERT_EQUAL(30, getConstValueAs<i64>("F", root_scope));

		// Floating point.
		ASSERT_EQUAL(1.0f, getConstValueAs<f64>("F1", root_scope));
		ASSERT_EQUAL(1.0l, getConstValueAs<f32>("F2", root_scope));
		ASSERT_EQUAL(5.0l, getConstValueAs<f64>("F3", root_scope));

		ASSERT_EQUAL(true, getConstValueAs<bool>("BOOL_TRUE", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("BOOL_FALSE", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("LOGIC_AND", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("LOGIC_OR", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("TRUE_COMPARISON", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("FALSE_COMPARISON", root_scope));

		ASSERT_EQUAL(42, getConstValueAs<i64>("VM_SIMPLE_CALL", root_scope));
		ASSERT_EQUAL(1'129, getConstValueAs<i64>("VM_SIMPLE_CALL_2", root_scope));
		ASSERT_EQUAL(55, getConstValueAs<i64>("FIB_10", root_scope));
		ASSERT_EQUAL(55, getConstValueAs<i32>("FIB_ON_I32_10", root_scope));
		ASSERT_EQUAL(58, getConstValueAs<i64>("COMPLEX_VM_CALL", root_scope));
		ASSERT_EQUAL(37, getConstValueAs<i64>("COMPLEX_VM_CALL_2", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("COLLATZ", root_scope));

		// Meta builtins (size_of / alignment_of) called through a VM comp-time function call.
		ASSERT_EQUAL(16, getConstValueAs<i64>("SIZE_AND_ALIGN_I64", root_scope));

		ASSERT_EQUAL(7, getConstValueAs<i64>("REF_POINT_SUM", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("VALUE_POINT_SUM", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("POINT_X", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("POINT_FIELDS_SUM", root_scope));
		ASSERT_EQUAL(5, getConstValueAs<i64>("TUPLE_ELEM", root_scope));
		ASSERT_EQUAL(17, getConstValueAs<i64>("MATCHED_VARIANT", root_scope));

		ASSERT_EQUAL(60, getConstValueAs<i64>("REF_ARRAY_SUM", root_scope));
		ASSERT_EQUAL(20, getConstValueAs<i64>("ARRAY_ELEM", root_scope));
		ASSERT_EQUAL(30, getConstValueAs<i64>("ARRAY_ELEM_OF_CALL", root_scope));
	}

	/**
	 * The compile-time DVM buffers whatever the evaluated code prints, so a `print` inside a
	 * compile-time evaluation used to be dropped. It now ends up on `std::cerr` instead.
	 */
	void testCompTimeOutput() {
		auto module     = compiler::driver::test_utils::getModuleIdFromPath("comp_time_output");
		auto root_scope = getModuleScope(module);

		std::ostringstream captured_cerr;
		auto*              real_cerr_buffer = std::cerr.rdbuf(captured_cerr.rdbuf());
		defer(std::cerr.rdbuf(real_cerr_buffer));

		// Evaluating the const runs `printAtCompTime` on the compile-time DVM.
		ASSERT_EQUAL(1, getConstValueAs<i64>("printed_at_comp_time", root_scope));

		ASSERT_TRUE(captured_cerr.str().contains("[comp-time] 3492"));
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
				= dynamic_cast<const CallExpr*>(stripImplicitMove(prepended_stmt.initial_value.get()
			    ));
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
				= dynamic_cast<const CallExpr*>(stripImplicitMove(appended_stmt.initial_value.get())
			    );
			const auto appended_callee = dynamic_cast<IdentifierExpr*>(appended_expr->callee.get());
			assertEqual(
				compiler::helios::name(appended_callee->symbol),
				base::StrID(":+"),
				"The appended expression should call the `:+` operator"
			);
		}

		{
			const auto& concatenated_stmt = dynamic_cast<const VariableStmt&>(*statements.at(5));
			const auto  concatenated_expr = dynamic_cast<const CallExpr*>(
                stripImplicitMove(concatenated_stmt.initial_value.get())
            );
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
			const auto  format_expr
				= dynamic_cast<const CallExpr*>(stripImplicitMove(format_stmt.initial_value.get()));
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
			const auto i64_type        = getIntegralTypeNoContext(64, Signed);
			const auto char_slice_type = getSliceTypeNoContext(st(char_type));

			auto fun_body_scope = getFunctionBodyScope(fun_sym);

			// Vars
			ASSERT_EQUAL(char_slice_type, getTypeOf("should_char_slice", fun_body_scope));
			ASSERT_EQUAL(char_type, getTypeOf("should_char", fun_body_scope));
			ASSERT_EQUAL(i64_type, getTypeOf("should_i64", fun_body_scope));
			ASSERT_EQUAL(str_type, getTypeOf("should_string", fun_body_scope));

			// Every kind of type stringifies, and always into a `String`.
			static constexpr std::array VARS
				= { "unit_string",         "bool_string",    "char_string",      "integral_string",
				    "unsigned_string",     "float_string",   "slice_string",     "class_string",
				    "cptr_string",         "manyptr_string", "int_slice_string", "array_string",
				    "nested_array_string", "tuple_string",   "variant_string",   "list_string" };

			for (const char* stringified: VARS)
				assertEqual(
					str_type,
					getTypeOf(stringified, fun_body_scope),
					base::strConcat("`", stringified, "` should be a `String`")
				);

			// Each of those calls must resolve to a `toString` whose body the compiler really
			// generates, not just to a declaration that type checks.
			query::utils::withContextDo([&](query::Context& ctx) {
				usize checked = 0;
				for (const auto& stmt: statements) {
					const auto* var_stmt = dynamic_cast<const VariableStmt*>(stmt.get());
					if (var_stmt == nullptr) continue;

					const auto var_name = compiler::helios::name(var_stmt->helios_symbol);
					if (not std::ranges::contains(VARS, var_name.strView())) continue;

					const auto* call = dynamic_cast<const CallExpr*>(
						stripImplicitMove(var_stmt->initial_value.get())
					);
					assertTrue(
						call != nullptr,
						base::strConcat("`", var_name, "` should be initialized with a call")
					);

					const auto to_string_sym
						= compiler::helios::getIdentifierExprSymID(call->callee.ref()).value();
					assertEqual(
						base::StrID("toString"),
						compiler::helios::name(to_string_sym),
						base::strConcat("`", var_name, "` should call `toString`")
					);
					assertTrue(
						compiler::helios::implementsQueryCodeOfFun(to_string_sym),
						base::strConcat("`", var_name, "`'s `toString` should be implemented")
					);

					const auto& to_string_fun
						= ctx.query<compiler::helios::QueryCodeOfFun>(to_string_sym)->valueOrThrow();
					assertTrue(
						not to_string_fun.body->statements.empty(),
						base::strConcat("`", var_name, "`'s `toString` should have a body")
					);
					++checked;
				}
				assertEqual(
					VARS.size(), checked, "Every stringified variable should have been checked"
				);
			});
		}
	}

	/**
	 * Test the origin calculation for elements.
	 * The origin calculation is not trivial for expressions and generated elements.
	 *
	 * This one lives here rather than in `helios_test.cpp` because its module holds a
	 * `box`: allocating a box goes through the `boxAlloc` primitive of `core.containers`,
	 * so the standard library has to be loaded.
	 */
	void testHoutElementsOrigin() {
		auto [module, _] = getModule(fs::File(path("test_modules/helios_pst_origin_tests")));

		auto& hout = query::entryPoint<compiler::helios::QueryModuleHOUT>(module)->valueOrPanic();

		auto get_global_by_name
			= [&](base::StrID name) -> base::Optional<CRef<compiler::helios::HOUTGlobalData>> {
			for (const auto& glob: hout.glob_data)
				if (glob->original_name == name) return glob;
			return {};
		};

		auto main_file = query::entryPoint<compiler::frontend::QueryMainSourceFile>({ module });
		base::Optional<CRef<pst::PST<>>> pst;
		query::utils::withContextDo([&](::query::Context& ctx) { pst = getFilePST(ctx, main_file); }
		);
		auto all_variables
			= pst::viewAllSubTreeElementsFilter<pst::Variable>(pst.value()->getRootElement());

		auto get_pst_variable_by_name
			= [&](base::StrID name) -> base::Optional<pst::Access<pst::Variable>> {
			for (const auto& var: all_variables)
				if (var.illegalAccess().value()->getName().illegalAccess().value()->unwrap() == name)
					return var.illegalAccess();
			return {};
		};

		auto get_source_pos_from_origin
			= [](const compiler::helios::code::ElementOrigin& origin) -> dia::SourcePosition {
			return std::any_cast<dia::SourcePosition>(query::utils::withContextCompute(
				[&](query::Context& ctx) { return origin.getSourcePosition(ctx).value(); }
			));
		};

		/**
		 * Check if the source position of the origin of initial value expression
		 * calculated from HOUT is the same as the source position of the whole PST init expression.
		 * This is not trivial for AccessChain and for generated elements. (derefs, casts)
		 */
		auto check_var_init_expr_origin = [&](base::StrID name, bool is_generated) {
			auto glob_opt = get_global_by_name(name);
			auto var_opt  = get_pst_variable_by_name(name);
			ASSERT_HAS_VALUE(glob_opt, "Global not found in HOUT");
			ASSERT_HAS_VALUE(var_opt, "Variable not found in PST");
			auto glob              = glob_opt.value();
			auto var_pst           = var_opt.value();
			auto initial_value_pst = var_pst->getValue().value().illegalAccess().value();
			auto initial_value_expr
				= std::get<compiler::helios::HOUTGlobalVariable>(glob->value).initial_value.ref();

			auto expr_pos_from_origin = get_source_pos_from_origin(initial_value_expr->origin);
			assertEqual(
				expr_pos_from_origin,
				initial_value_pst->getSourcePosition().illegalAccess(),
				base::strConcat(
					"The initial value expression PST node does not match for variable ",
					name.strView()
				)
			);

			assertEqual(
				initial_value_expr->origin.isGenerated(),
				is_generated,
				base::strConcat("Generated origin does not match for variable ", name.strView())
			);
		};
		check_var_init_expr_origin(base::StrID("var1"), false);
		check_var_init_expr_origin(base::StrID("var2"), false);
		check_var_init_expr_origin(base::StrID("var3"), false);
		check_var_init_expr_origin(base::StrID("var4"), false);
		check_var_init_expr_origin(base::StrID("var5"), false);
		check_var_init_expr_origin(base::StrID("var6"), false);
		check_var_init_expr_origin(base::StrID("var7_generated"), true);
		check_var_init_expr_origin(base::StrID("var8_generated"), true);

		auto get_hout_function_by_name
			= [&](base::StrID name) -> base::Optional<CRef<compiler::helios::HOUTFunction>> {
			for (const auto& fun: hout.functions)
				if (fun->declaration->original_name == name) return fun;
			return {};
		};
		auto all_pst_functions
			= pst::viewAllSubTreeElementsFilter<pst::Fun>(pst.value()->getRootElement());
		auto get_pst_function_by_name
			= [&](base::StrID name) -> base::Optional<pst::Access<pst::Fun>> {
			for (const auto& fun: all_pst_functions)
				if (fun.illegalAccess().value()->getName().illegalAccess().value()->unwrap() == name)
					return fun.illegalAccess();
			return {};
		};

		/**
		 * Check if the source position of the origin of function body calculated from HOUT
		 * is the same as the source position of the whole PST function of the same name.
		 */
		auto check_function_origin = [&](base::StrID name) {
			auto fun_opt     = get_hout_function_by_name(name);
			auto pst_fun_opt = get_pst_function_by_name(name);
			ASSERT_HAS_VALUE(fun_opt, "Function not found in HOUT");
			ASSERT_HAS_VALUE(pst_fun_opt, "Function not found in PST");
			auto fun = fun_opt.value();


			auto pst_fun = pst_fun_opt.value();
			assertEqual(
				get_source_pos_from_origin(fun->origin),
				pst_fun->getSourcePosition().illegalAccess(),
				base::strConcat("The function origin is not the PST of the function", name.strView())
			);
			assertEqual(
				fun->origin.getPSTElement().value().illegalAccess().value()->getHash(),
				pst_fun->getHash(),
				base::strConcat(
					"The function origin PST element does not match the PST function for ",
					name.strView()
				)
			);
		};

		check_function_origin(base::StrID("a"));
		check_function_origin(base::StrID("b"));

		query::utils::withContextDo([&](query::Context& ctx) { hout.debugPrint(ctx, std::cout); });
	}

	void testBoxes() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/boxes")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(4);
		auto& body     = *function->body;
		using namespace compiler::helios::code;

		{
			// var b_int: box i32 = 42;
			ASSERT_TRUE(body.statements.size() > 0);
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[0].get());
			ASSERT_TRUE(var_stmt != nullptr);

			auto* boxed_value = boxAllocArg(stripImplicitMove(var_stmt->initial_value.get()));
			ASSERT_TRUE(boxed_value != nullptr);

			auto* literal_expr = dynamic_cast<const LiteralNumericExpr*>(boxed_value);
			ASSERT_TRUE(literal_expr != nullptr);

			auto var_type = var_stmt->type;
			ASSERT_EQUAL(var_type.getRefKind(), compiler::tsh::ReferenceKind::Box);
			ASSERT_EQUAL(var_type.getType().getKind(), compiler::tsh::Kind::Integral);
		}
		{
			// take_int_ref(&b_int);
			// `&` on `box i32` creates a `ref i32`
			ASSERT_TRUE(body.statements.size() > 2);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[1].get());
			ASSERT_TRUE(expr_stmt != nullptr);
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* ref_of = dynamic_cast<const RefOfExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(ref_of != nullptr);
		}
		{
			// var x: i32 = b_point.x;
			ASSERT_TRUE(body.statements.size() > 5);
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[4].get());
			ASSERT_TRUE(var_stmt != nullptr);

			auto* access_expr
				= dynamic_cast<const AccessExpr*>(stripImplicitMove(var_stmt->initial_value.get()));
			ASSERT_TRUE(access_expr != nullptr);
			ASSERT_EQUAL(compiler::helios::name(access_expr->field), "x");

			auto* deref_expr = dynamic_cast<const DerefExpr*>(access_expr->base.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// b_point.y = 99;
			ASSERT_TRUE(body.statements.size() > 7);
			auto* assign_stmt = dynamic_cast<const AssignmentStmt*>(body.statements[6].get());
			ASSERT_TRUE(assign_stmt != nullptr);

			auto* access_expr = dynamic_cast<const AccessExpr*>(assign_stmt->location_expr.get());
			ASSERT_TRUE(access_expr != nullptr);
			ASSERT_EQUAL(compiler::helios::name(access_expr->field), "y");

			auto* deref_expr = dynamic_cast<const DerefExpr*>(access_expr->base.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// by_val(b_point);
			// `box T -> T`
			ASSERT_TRUE(body.statements.size() > 8);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[7].get());
			ASSERT_TRUE(expr_stmt != nullptr);
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* deref_expr = dynamic_cast<const DerefExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(deref_expr != nullptr);

			auto* ident_expr = dynamic_cast<const IdentifierExpr*>(deref_expr->inner.get());
			ASSERT_TRUE(ident_expr != nullptr);
		}
		{
			// by_ref(b_point);
			// `box T -> ref T`
			ASSERT_TRUE(body.statements.size() > 9);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[8].get());
			ASSERT_TRUE(expr_stmt != nullptr);

			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* refof_expr = dynamic_cast<const RefOfExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(refof_expr != nullptr);

			auto* ident_expr = dynamic_cast<const IdentifierExpr*>(refof_expr->inner.get());
			ASSERT_TRUE(ident_expr != nullptr);
		}
		{
			// `box i32 -> i32` in return.
			auto* return_stmt = dynamic_cast<const ReturnStmt*>(body.statements.back().get());
			ASSERT_TRUE(return_stmt != nullptr);

			auto* deref_expr = dynamic_cast<const DerefExpr*>(return_stmt->value.get());
			ASSERT_TRUE(deref_expr != nullptr);

			auto* ident_expr
				= dynamic_cast<const IdentifierExpr*>(stripImplicitMove(deref_expr->inner.get()));
			ASSERT_TRUE(ident_expr != nullptr);
		}
	}

	void testReferenceKindCollapsing() {
		auto [module, top_scope]
			= getModule(fs::File(path("test_modules/reference_kind_collapsing")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(0);
		auto& body     = *function->body;
		using namespace compiler::helios::code;

		auto i32_type
			= getIntegralTypeNoContext(32, compiler::tsh::IntegralAbstractType::Signedness::Signed);
		auto ref_i32 = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
		auto box_i32 = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);

		auto get_var_stmt = [&](usize index) -> const VariableStmt& {
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[index].get());
			ASSERT_TRUE(var_stmt != nullptr);
			return *var_stmt;
		};

		// var ref_ref_a: ref i32 = &ref_a; (Ref -> Ref)
		{
			const auto& var_stmt = get_var_stmt(3);
			ASSERT_EQUAL(var_stmt.type, ref_i32);
			// `&ref_a` should just copy the pointer, which is a simple assignment.
			// The explicit `&` creates a RefOfExpr, and type system collapses the type.
			auto* ref_of
				= dynamic_cast<const RefOfExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var ref_box_a: ref i32 = &box_a; (Box -> Ref)
		{
			const auto& var_stmt = get_var_stmt(4);
			ASSERT_EQUAL(var_stmt.type, ref_i32);
			auto* ref_of
				= dynamic_cast<const RefOfExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var box_ref_a: box i32 = ref_a; (Ref -> Box)
		{
			const auto& var_stmt = get_var_stmt(5);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// This should create a copy. `boxAlloc(DerefExpr(...))`
			auto* boxed_value = boxAllocArg(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(boxed_value != nullptr);
			auto* deref = dynamic_cast<const DerefExpr*>(boxed_value);
			ASSERT_TRUE(deref != nullptr);
		}
		// var box_box_a: box i32 = move box_a; (Box -> Box)
		{
			const auto& var_stmt = get_var_stmt(6);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// `box = box` needs an explicit `move`.
			auto* move_expr
				= dynamic_cast<const MoveExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(move_expr != nullptr);
			auto* ident = dynamic_cast<const IdentifierExpr*>(move_expr->inner.get());
			ASSERT_TRUE(ident != nullptr);
		}
	}

	void testCopyConstructors() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/copy_constructors")));

		auto trivial_sym = getChain("Trivial", root_scope).back();
		auto has_box_sym = getChain("HasBox", root_scope).back();
		auto holds_sym   = getChain("HoldsNonTrivial", root_scope).back();
		auto mega_sym    = getChain("FinalBoss", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type
				= [&](SymID sym_id) { return ctx.query<compiler::tsh::QueryClassType>(sym_id); };

			// Classes, tuples and static arrays are copied by a single `create_aggregate` that the
			// body returns, with one value per copied element.
			auto returned_aggregate = [](const HOUTFunction& cctor) -> const CreateAggregateExpr& {
				auto ret = dynamic_cast<const ReturnStmt*>(cctor.body->statements.back().get());
				CORE_ASSERT(ret != nullptr, "The copy constructor's body must end with a return.");
				auto aggregate
					= dynamic_cast<const CreateAggregateExpr*>(stripImplicitMove(ret->value.get()));
				CORE_ASSERT(aggregate != nullptr, "The copy constructor must return an aggregate.");
				return *aggregate;
			};

			// The symbol of the copy constructor invoked by a copy-ctor-call expression.
			auto callee_of = [&](const Expr* expr) -> SymID {
				auto call = dynamic_cast<const CallExpr*>(stripImplicitMove(expr));
				ASSERT_TRUE(call != nullptr);
				return getIdentifierExprSymID(call->callee.ref()).value();
			};

			auto assert_generated_copy = [&](const Expr* expr) {
				const auto* ctor = std::get_if<Constructor>(&getSymRef(callee_of(expr))->other);
				ASSERT_TRUE(ctor != nullptr);
				ASSERT_EQUAL(ctor->kind, Constructor::Kind::Copy);
			};

			// For a trivially-copyable class, the copy constructor copies each field with byte
			// copy, so each value of the aggregate is a field access, not a copy-ctor call.
			{
				auto        trivial_type = get_class_type(trivial_sym);
				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(trivial_type)->valueOrThrow();

				// `(const ref Trivial) -> Trivial`.
				ASSERT_EQUAL_PRINT(1, cctor.declaration->parameters.size());
				const auto param_type = cctor.declaration->parameters.at(0).type;
				ASSERT_EQUAL(compiler::tsh::ReferenceKind::Ref, param_type.getRefKind());
				ASSERT_EQUAL(compiler::tsh::Mutability::Immutable, param_type.getMutability());
				ASSERT_EQUAL(trivial_type, param_type.getType());
				ASSERT_EQUAL(trivial_type, cctor.declaration->return_type.getType());

				// return create_aggregate(Trivial) { (*source).a, (*source).b };
				ASSERT_EQUAL_PRINT(1, cctor.body->statements.size());
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(1).get())) != nullptr
				);
			}

			// For a class holding a non-trivially-copyable field, the copy constructor copies the
			// field by calling that field type's copy constructor.
			{
				auto        holds_type = get_class_type(holds_sym);
				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(holds_type)->valueOrThrow();

				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				assert_generated_copy(stripImplicitMove(values.at(0).get()));
			}

			// A tuple is copied element-by-element just like a class. Trivial elements are
			// byte-copied, non-trivially-copyable elements are copied with their own copy constructor.
			{
				auto i32_type = compiler::tsh::getIntegralType(
					ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
				);
				auto has_box_st = st(get_class_type(has_box_sym));
				auto tuple_type
					= ctx.query<compiler::tsh::QueryTupleType>({ { st(i32_type), has_box_st } });

				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(tuple_type)->valueOrThrow();

				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());

				// First element is trivially copyable, the second one requires a copy ctor.
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				assert_generated_copy(stripImplicitMove(values.at(1).get()));
			}

			auto dump_cctor = [&](std::string_view            label,
			                      compiler::tsh::AbstractType type) -> const HOUTFunction& {
				const auto& cctor = ctx.query<QueryDefaultCopyConstructor>(type)->valueOrThrow();
				std::cerr << "\n===== copy constructor: " << label << " =====\n";
				for (const auto& stmt: cctor.body->statements) {
					stmt->debugPrint(std::cerr, 1);
					std::cerr << "\n";
				}
				return cctor;
			};

			const auto  final_boss_type  = get_class_type(mega_sym);
			const auto& final_boss_cctor = dump_cctor("FinalBoss", final_boss_type);

			// The value of the aggregate that copies field `field_name`. The values are in field
			// declaration order, one per field.
			auto rhs_of = [&](std::string_view field_name) -> const Expr* {
				const auto& values = returned_aggregate(final_boss_cctor).values;
				usize       index  = 0;
				for (const auto& field: final_boss_type.getInterface(ctx)->getFieldsView()) {
					if (compiler::helios::name(field.getSymbol()).strView() == field_name)
						return values.at(index).get();
					index++;
				}
				CORE_PANIC("no value found for field");
			};

			// Trivially-copyable fields are byte-copied. The RHS is should be a plain field access.
			for (std::string_view trivial_field:
			     { "i", "f", "flag", "r", "p", "c", "m", "trivial_arr", "trivial_tup" })
				ASSERT_TRUE(dynamic_cast<const AccessExpr*>(rhs_of(trivial_field)) != nullptr);

			// `box i32` - deep copy of a trivial pointee -> boxAlloc(*source.boxed_prim).
			{
				auto boxed = boxAllocArg(rhs_of("boxed_prim"));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_TRUE(dynamic_cast<const DerefExpr*>(boxed) != nullptr);
			}

			// `box HasBox` - deep copy of a non-trivial pointee -> boxAlloc(HasBox.__copy(...)).
			{
				auto boxed = boxAllocArg(rhs_of("boxed_class"));
				ASSERT_TRUE(boxed != nullptr);
				assert_generated_copy(boxed);
			}

			// Non-trivial aggregates call a copy constructor.
			for (std::string_view aggregate_field:
			     { "nontrivial_arr", "nontrivial_tup", "nested_default" })
				assert_generated_copy(rhs_of(aggregate_field));

			// A field whose class defines a user copy constructor calls the user code, not a
			// generated one.
			ASSERT_MATCHES(getSymRef(callee_of(rhs_of("nested_user")))->other, ClassMemberSemantics);

			// `box UserCopied` - deep copy whose inner pointee copy runs the user constructor.
			{
				auto boxed = boxAllocArg(rhs_of("deep"));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_MATCHES(getSymRef(callee_of(boxed))->other, ClassMemberSemantics);
			}

			auto field_abstract_type = [&](std::string_view field_name) {
				return final_boss_type.getInterface(ctx)
				    ->getElementsWithName(base::StrID(field_name))
				    .back()
				    .getType(ctx)
				    .getType();
			};

			// HasBox -> boxAlloc(*source.boxed).
			{
				const auto& cctor  = dump_cctor("HasBox", get_class_type(has_box_sym));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				auto boxed = boxAllocArg(stripImplicitMove(values.at(0).get()));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_TRUE(dynamic_cast<const DerefExpr*>(boxed) != nullptr);
			}

			// HoldsNonTrivial - copies its `HasBox` field with a copy-ctor call.
			{
				const auto& cctor  = dump_cctor("HoldsNonTrivial", get_class_type(holds_sym));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				assert_generated_copy(stripImplicitMove(values.at(0).get()));
			}

			// Static array - one value copying the element the index variable points at, with the
			// per-element statements advancing that index.
			{
				const auto& cctor = dump_cctor("HasBox[3]", field_abstract_type("nontrivial_arr"));
				const auto& stmts = cctor.body->statements;

				// var __i = 0;
				// return create_aggregate(HasBox[3]) [ (*source)[__i].__copy() ]
				//     per_element { __i = __i + 1; };
				ASSERT_EQUAL_PRINT(2, stmts.size());
				ASSERT_TRUE(dynamic_cast<const VariableStmt*>(stmts.front().get()) != nullptr);

				const auto& aggregate = returned_aggregate(cctor);
				ASSERT_EQUAL_PRINT(1, aggregate.values.size());
				assert_generated_copy(stripImplicitMove(aggregate.values.at(0).get()));

				ASSERT_HAS_VALUE(aggregate.per_element_body);
				const auto& per_element = (*aggregate.per_element_body)->statements;
				ASSERT_EQUAL_PRINT(1, per_element.size());
				ASSERT_TRUE(dynamic_cast<const AssignmentStmt*>(per_element.at(0).get()) != nullptr);
			}

			// Tuple with a non-trivial element - element 0 byte-copied, element 1 via a copy ctor.
			{
				const auto& cctor
					= dump_cctor("(i32, HasBox)", field_abstract_type("nontrivial_tup"));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				assert_generated_copy(stripImplicitMove(values.at(1).get()));
			}

			// A variant is copied by matching the source and rebuilding the variant around a copy
			// of the active alternative, so every alternative gets a case and every case result is
			// a variant construction of that same alternative.
			{
				auto i32_type = compiler::tsh::getIntegralType(
					ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
				);
				auto variant_type = ctx.query<compiler::tsh::QueryVariantType>({
					{ st(i32_type), st(get_class_type(has_box_sym)) },
				});

				const auto& cctor = dump_cctor("i32 | HasBox", variant_type);

				// `(const ref V) -> V`.
				ASSERT_EQUAL_PRINT(1, cctor.declaration->parameters.size());
				const auto param_type = cctor.declaration->parameters.at(0).type;
				ASSERT_EQUAL(compiler::tsh::ReferenceKind::Ref, param_type.getRefKind());
				ASSERT_EQUAL(compiler::tsh::Mutability::Immutable, param_type.getMutability());
				ASSERT_EQUAL(variant_type, param_type.getType());
				ASSERT_EQUAL(variant_type, cctor.declaration->return_type.getType());

				// return match (source) { <one case per alternative> };
				ASSERT_EQUAL_PRINT(1, cctor.body->statements.size());
				auto ret = dynamic_cast<const ReturnStmt*>(cctor.body->statements.back().get());
				ASSERT_TRUE(ret != nullptr);
				auto match = dynamic_cast<const MatchExpr*>(stripImplicitMove(ret->value.get()));
				ASSERT_TRUE(match != nullptr);

				const auto& alternatives = variant_type.getUnderlyingTypes();
				ASSERT_EQUAL_PRINT(alternatives.size(), match->cases.size());

				// The cases survive a clone, which the lowering relies on to know how a payload
				// is bound.
				auto cloned_expr = match->clone();
				auto cloned      = dynamic_cast<const MatchExpr*>(cloned_expr.get());
				ASSERT_TRUE(cloned != nullptr);
				ASSERT_EQUAL_PRINT(match->cases.size(), cloned->cases.size());

				// Every case tests its own alternative, binds the payload, and rebuilds the
				// variant with that same alternative index - no wildcard is needed.
				for (usize i = 0; i < match->cases.size(); i++) {
					const auto& match_case = match->cases.at(i);
					ASSERT_HAS_VALUE(match_case.alternative_index);
					ASSERT_EQUAL_PRINT(i, match_case.alternative_index.value());
					ASSERT_HAS_VALUE(match_case.binding);

					// The subject is borrowed, so the constraint is the type the payload is
					// bound with: a mutable reference to the alternative.
					ASSERT_TRUE(match_case.constraint_type.has_value());
					ASSERT_EQUAL(
						alternatives.at(i)
							.withReferenceKind(compiler::tsh::ReferenceKind::Ref)
							.withMutability(compiler::tsh::Mutability::Mutable),
						match_case.constraint_type.value()
					);
					ASSERT_TRUE(cloned->cases.at(i).constraint_type.has_value());
					ASSERT_EQUAL(
						match_case.constraint_type.value(),
						cloned->cases.at(i).constraint_type.value()
					);

					auto construct = dynamic_cast<const VariantConstructExpr*>(
						stripImplicitMove(match_case.result.get())
					);
					ASSERT_TRUE(construct != nullptr);
					ASSERT_EQUAL_PRINT(i, construct->alternative_index);

					// The alternatives are ordered by their name, so `HasBox` comes before `i32`.
					// The non-trivially-copyable one is copied with its copy constructor, the
					// trivially-copyable one is byte-copied from the dereferenced binding.
					if (alternatives.at(i).isTriviallyCopyable(ctx))
						ASSERT_TRUE(
							dynamic_cast<const DerefExpr*>(stripImplicitMove(construct->inner.get()))
							!= nullptr
						);
					else
						assert_generated_copy(stripImplicitMove(construct->inner.get()));
				}
			}
		});
	}

	void testDestructors() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/destructors")));

		auto trivial_sym      = getChain("Trivial", root_scope).back();
		auto has_box_sym      = getChain("HasBox", root_scope).back();
		auto user_sym         = getChain("UserDestroyed", root_scope).back();
		auto holds_sym        = getChain("HoldsNonTrivial", root_scope).back();
		auto user_members_sym = getChain("UserAndMembers", root_scope).back();
		auto boss_sym         = getChain("FinalBoss", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type
				= [&](SymID sym_id) { return ctx.query<compiler::tsh::QueryClassType>(sym_id); };

			auto is_method_call_expr = [&](const Expr* expr, Method::Kind kind) -> bool {
				auto call = dynamic_cast<const CallExpr*>(expr);
				if (call == nullptr) return false;
				auto callee = getIdentifierExprSymID(call->callee.ref());
				if (!callee.has_value()) return false;
				const auto* method = std::get_if<Method>(&getSymRef(callee.value())->other);
				return method != nullptr && method->kind == kind;
			};

			auto is_method_call = [&](const Stmt* stmt, Method::Kind kind) -> bool {
				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmt);
				if (expr_stmt == nullptr) return false;
				return is_method_call_expr(expr_stmt->expr.get(), kind);
			};

			// Returns the callee symbol of a statement of the form `f(...);`, if any.
			auto call_callee = [&](const Stmt* stmt) -> base::Optional<SymID> {
				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmt);
				if (expr_stmt == nullptr) return {};
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				if (call == nullptr) return {};
				return getIdentifierExprSymID(call->callee.ref());
			};

			// A `box T` is released by the `boxFree` primitive of `core.containers`, baked for
			// the pointee type.
			auto is_box_free_call = [&](const Stmt* stmt) -> bool {
				auto callee = call_callee(stmt);
				if (!callee.has_value()) return false;
				return compiler::helios::name(callee.value()) == base::StrID("boxFree");
			};

			// A trivially-destructible class has an empty destructor body.
			{
				const auto  type = get_class_type(trivial_sym);
				const auto& dtor = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();

				// `(ref mut Trivial) -> ()`.
				ASSERT_EQUAL_PRINT(1, dtor.declaration->parameters.size());
				const auto self_type = dtor.declaration->parameters.at(0).type;
				ASSERT_EQUAL(compiler::tsh::ReferenceKind::Ref, self_type.getRefKind());
				ASSERT_EQUAL(compiler::tsh::Mutability::Mutable, self_type.getMutability());
				ASSERT_EQUAL(type, self_type.getType());
				ASSERT_EQUAL(
					compiler::tsh::Kind::Unit, dtor.declaration->return_type.getType().getKind()
				);

				ASSERT_TRUE(dtor.body->statements.empty());
				ASSERT_TRUE(type.isTriviallyDestructible(ctx));
			}

			// A class owning a `box i32` destroys it with a single `boxFree` call, which takes
			// care of both the pointee and the storage.
			{
				const auto type = get_class_type(has_box_sym);
				ASSERT_TRUE(!type.isTriviallyDestructible(ctx));

				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_box_free_call(stmts.at(0).get()));
			}

			// A class holding a non-trivially-destructible member destroys it via that member's own
			// destructor.
			{
				const auto  type  = get_class_type(holds_sym);
				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_method_call(stmts.at(0).get(), Method::Kind::DefaultDestructor));
			}

			// A class that declares a user destructor should call the user code first.
			{
				const auto type = get_class_type(user_sym);
				ASSERT_TRUE(!type.isTriviallyDestructible(ctx));

				const auto user_dtor_element = type.getInterface(ctx)->getSpecialElement(
					compiler::tsh::MemberSpecialKind::UserDestructor
				);
				ASSERT_HAS_VALUE(user_dtor_element);
				const auto user_dtor = user_dtor_element.value()->getSymbol();
				ASSERT_TRUE(isUserDefinedDestructor(ctx, user_dtor));
				ctx.query<QueryCodeOfFun>(user_dtor)->valueOrThrow();

				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());

				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmts.at(0).get());
				ASSERT_TRUE(expr_stmt != nullptr);
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				ASSERT_TRUE(call != nullptr);
				ASSERT_EQUAL(user_dtor, getIdentifierExprSymID(call->callee.ref()).value());
			}

			// A class with a user destructor and non-trivial members should call the user code
			// first, then destroy the members in reverse order.
			{
				const auto  type  = get_class_type(user_members_sym);
				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(3, stmts.size());

				// [0] user destructor call.
				const auto user_dtor
					= type.getInterface(ctx)
				          ->getSpecialElement(compiler::tsh::MemberSpecialKind::UserDestructor)
				          .value()
				          ->getSymbol();
				auto user_call = dynamic_cast<const CallExpr*>(
					dynamic_cast<const ExprStmt*>(stmts.at(0).get())->expr.get()
				);
				ASSERT_TRUE(user_call != nullptr);
				ASSERT_EQUAL(user_dtor, getIdentifierExprSymID(user_call->callee.ref()).value());

				// [1] `second` (box i32) released by `boxFree`
				ASSERT_TRUE(is_box_free_call(stmts.at(1).get()));
				// [2] `first` (HasBox) destroyed
				ASSERT_TRUE(is_method_call(stmts.at(2).get(), Method::Kind::DefaultDestructor));
			}

			// A variant destroys only the alternatives that own something, by matching the
			// active one. The alternatives are ordered by name, so `HasBox` comes first.
			{
				auto i32_type = compiler::tsh::getIntegralType(
					ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
				);
				const auto variant_type = ctx.query<compiler::tsh::QueryVariantType>({
					{ st(i32_type), st(get_class_type(has_box_sym)) },
				});
				ASSERT_TRUE(!variant_type.isTriviallyDestructible(ctx));

				const auto& dtor  = ctx.query<QueryDefaultDestructor>(variant_type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());

				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmts.at(0).get());
				ASSERT_TRUE(expr_stmt != nullptr);
				auto match = dynamic_cast<const MatchExpr*>(expr_stmt->expr.get());
				ASSERT_TRUE(match != nullptr);

				// One case for the owning alternative, plus the wildcard that keeps the match
				// exhaustive over the trivially destructible one.
				ASSERT_EQUAL_PRINT(2, match->cases.size());

				const auto& owning_case = match->cases.at(0);
				ASSERT_TRUE(owning_case.alternative_index.has_value());
				ASSERT_EQUAL_PRINT(0, owning_case.alternative_index.value());
				ASSERT_TRUE(owning_case.binding.has_value());
				// The payload is destroyed in place, so it is bound as a mutable reference.
				ASSERT_TRUE(owning_case.constraint_type.has_value());
				ASSERT_EQUAL(
					variant_type.getUnderlyingTypes()
						.at(0)
						.withReferenceKind(compiler::tsh::ReferenceKind::Ref)
						.withMutability(compiler::tsh::Mutability::Mutable),
					owning_case.constraint_type.value()
				);
				ASSERT_TRUE(
					is_method_call_expr(owning_case.result.get(), Method::Kind::DefaultDestructor)
				);

				// The wildcard names no alternative, so it binds nothing and constrains nothing.
				const auto& wildcard_case = match->cases.at(1);
				ASSERT_TRUE(wildcard_case.alternative_index.empty());
				ASSERT_TRUE(wildcard_case.binding.empty());
				ASSERT_TRUE(wildcard_case.constraint_type.empty());
			}

			auto field_abstract_type = [&](std::string_view field_name) {
				return get_class_type(boss_sym)
				    .getInterface(ctx)
				    ->getElementsWithName(base::StrID(field_name))
				    .back()
				    .getType(ctx)
				    .getType();
			};

			// Static array of a non-trivial element - a destructor loop.
			{
				const auto  arr_type = field_abstract_type("nontrivial_arr");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(arr_type)->valueOrThrow();
				const auto& stmts    = dtor.body->statements;
				// var __i = 0; while (__i < 3) { ... }
				ASSERT_EQUAL_PRINT(2, stmts.size());
				ASSERT_TRUE(dynamic_cast<const WhileStmt*>(stmts.at(1).get()) != nullptr);
			}

			// Static array of a trivial element - empty destructor.
			{
				const auto  arr_type = field_abstract_type("trivial_arr");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(arr_type)->valueOrThrow();
				ASSERT_TRUE(dtor.body->statements.empty());
			}

			// Tuple with a non-trivial element - destroys that element via its destructor.
			{
				const auto  tup_type = field_abstract_type("nontrivial_tup");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(tup_type)->valueOrThrow();
				const auto& stmts    = dtor.body->statements;
				// Only the `HasBox` needs destruction.
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_method_call(stmts.at(0).get(), Method::Kind::DefaultDestructor));
			}
		});
	}

	void testCopyMoveOperators() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;

		auto [module, _] = getModule(fs::File(path("test_modules/copy_move_ops")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		const HOUTFunction* fn = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "usesOps") fn = &*f;
		ASSERT_TRUE(fn != nullptr);

		// var a = W(1);
		// var b = copy a;
		// var c = move a;
		// return c.x;
		const auto& stmts = fn->body->statements;
		ASSERT_EQUAL_PRINT(4, stmts.size());

		// `copy a` lowers to a copy ctor call.
		const auto* b_var = dynamic_cast<const VariableStmt*>(stmts.at(1).get());
		ASSERT_TRUE(b_var != nullptr);
		ASSERT_TRUE(
			dynamic_cast<const CallExpr*>(stripImplicitMove(b_var->initial_value.get())) != nullptr
		);

		// `move a` lowers to a MoveExpr.
		const auto* c_var = dynamic_cast<const VariableStmt*>(stmts.at(2).get());
		ASSERT_TRUE(c_var != nullptr);
		const auto* c_move
			= dynamic_cast<const MoveExpr*>(stripImplicitMove(c_var->initial_value.get()));
		ASSERT_TRUE(c_move != nullptr);
		ASSERT_EQUAL(MoveExpr::MoveKind::Explicit, c_move->kind);

		// `return w` should move the owned local implicitly out without a `move` keyword
		const HOUTFunction* returns_local = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "returnsLocal") returns_local = &*f;
		ASSERT_TRUE(returns_local != nullptr);

		const auto& return_stmts = returns_local->body->statements;
		const auto* ret_stmt     = dynamic_cast<const ReturnStmt*>(return_stmts.back().get());
		ASSERT_TRUE(ret_stmt != nullptr);

		const auto* implicit_move = dynamic_cast<const MoveExpr*>(ret_stmt->value.get());
		ASSERT_TRUE(implicit_move != nullptr);
		ASSERT_EQUAL(MoveExpr::MoveKind::Implicit, implicit_move->kind);
		ASSERT_TRUE(dynamic_cast<const IdentifierExpr*>(implicit_move->inner.get()) != nullptr);

		const HOUTFunction* through_ref = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "copiesThroughRef") through_ref = &*f;
		ASSERT_TRUE(through_ref != nullptr);

		// var b = copy r;   (r: ref W)
		// var c = copy bx;  (bx: box W)
		// Both should lower to a copy ctor call.
		const auto& ref_stmts = through_ref->body->statements;
		ASSERT_EQUAL_PRINT(3, ref_stmts.size());

		for (const usize i: { 0uz, 1uz }) {
			const auto* var_stmt = dynamic_cast<const VariableStmt*>(ref_stmts.at(i).get());
			ASSERT_TRUE(var_stmt != nullptr);
			ASSERT_TRUE(
				dynamic_cast<const CallExpr*>(stripImplicitMove(var_stmt->initial_value.get()))
				!= nullptr
			);
		}

		const HOUTFunction* keeps_kind = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "copyofKeepsKind") keeps_kind = &*f;
		ASSERT_TRUE(keeps_kind != nullptr);

		// var a = W(1);
		// var ca = copyof a;    (W)
		// var cr = copyof r;    (ref W)
		// var cb = copyof bx;   (box W)
		// var cn = copyof n;    (i64)
		// return ca.x;
		const auto& kind_stmts = keeps_kind->body->statements;
		ASSERT_EQUAL_PRINT(6, kind_stmts.size());

		const auto init_of = [&](const usize i) -> const Expr* {
			const auto* var_stmt = dynamic_cast<const VariableStmt*>(kind_stmts.at(i).get());
			ASSERT_TRUE(var_stmt != nullptr);
			return var_stmt->initial_value.get();
		};

		// `copyof a` on a direct value calls the copy constructor, just like `copy` does.
		const auto* ca_init = init_of(1);
		ASSERT_TRUE(dynamic_cast<const CallExpr*>(stripImplicitMove(ca_init)) != nullptr);
		ASSERT_EQUAL(
			compiler::tsh::ReferenceKind::Direct,
			ca_init->expression_type.getSymbolType().getRefKind()
		);

		// `copyof r` keeps the `ref`: the reference itself is copied, so there is no node.
		const auto* cr_init = init_of(2);
		ASSERT_TRUE(dynamic_cast<const IdentifierExpr*>(cr_init) != nullptr);
		ASSERT_EQUAL(
			compiler::tsh::ReferenceKind::Ref, cr_init->expression_type.getSymbolType().getRefKind()
		);

		// `copyof bx` keeps the `box`: a fresh allocation holding a copy-constructed pointee.
		const auto* cb_init = init_of(3);
		ASSERT_EQUAL(
			compiler::tsh::ReferenceKind::Box, cb_init->expression_type.getSymbolType().getRefKind()
		);
		const auto* boxed = boxAllocArg(cb_init);
		ASSERT_TRUE(boxed != nullptr);
		ASSERT_TRUE(dynamic_cast<const CallExpr*>(boxed) != nullptr);
	}

	void testHoutWalkers() {
		auto [module, scope] = getModule(fs::File(path("test_modules/function_calls")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		ASSERT_EQUAL(4, hout.functions.size());

		using namespace compiler::helios::code;

		auto square_symbol = getChain("square", scope).back();
		auto foo1_symbol   = getChain("foo1", scope).back();

		// Look up the HOUT functions by name so the test does not depend on emission order.
		auto find_fun = [&](std::string_view target) -> const auto& {
			for (const auto& fun: hout.functions)
				if (fun->declaration->original_name.strView() == target) return *fun;
			CORE_PANIC(base::strConcat("function not found in HOUT unit: ", target));
		};

		const auto& square = find_fun("square");
		const auto& foo    = find_fun("foo");
		const auto& main   = find_fun("main");

		// foo() calls square() twice; the collected list is deduplicated to one entry.
		auto called_from_fun = collectCalledSymbolsFromHOUT(foo);
		ASSERT_EQUAL(1, called_from_fun.size());
		ASSERT_EQUAL(square_symbol, called_from_fun.at(0));

		// The last statement is `return square(a_squared + b);` — walking just that
		// expression tree should also find the call to square().
		const auto& return_stmt = dynamic_cast<const ReturnStmt&>(*foo.body->statements.back());
		auto        called_from_expr = collectCalledSymbolsFromHOUT(*return_stmt.value);
		ASSERT_EQUAL(1, called_from_expr.size());
		ASSERT_EQUAL(square_symbol, called_from_expr.at(0));

		// square() is a leaf: it calls no other functions.
		auto called_from_square = collectCalledSymbolsFromHOUT(square);
		ASSERT_EQUAL(0, called_from_square.size());

		// main() calls foo1() four times (with different argument styles); the collected list is
		// still deduplicated to a single entry.
		auto called_from_main = collectCalledSymbolsFromHOUT(main);
		ASSERT_EQUAL(1, called_from_main.size());
		ASSERT_EQUAL(foo1_symbol, called_from_main.at(0));

		// Walking a single call statement's expression tree finds just that callee.
		const auto& first_stmt        = dynamic_cast<const ExprStmt&>(*main.body->statements.at(0));
		auto        called_from_first = collectCalledSymbolsFromHOUT(*first_stmt.expr);
		ASSERT_EQUAL(1, called_from_first.size());
		ASSERT_EQUAL(foo1_symbol, called_from_first.at(0));

		// `ptrof m[index()]` hides a call below the operand, so the walker only finds it if it
		// descends into `PtrOfExpr`'s child.
		auto [ptr_module, ptr_scope] = getModule(fs::File(path("test_modules/pointers")));
		auto& ptr_hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(ptr_module)->valueOrPanic();
		auto called_from_pointers = collectCalledSymbolsFromHOUT(*ptr_hout.functions.at(0));
		ASSERT_TRUE(std::ranges::contains(called_from_pointers, getChain("index", ptr_scope).back())
		);
	}

	void testPointers() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/pointers")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto test_pointers_sym = getChain("test_pointers", top_scope).back();
		auto body_scope        = getFunctionBodyScope(test_pointers_sym);

		auto i32_type = getIntegralTypeNoContext(32, Signed);
		auto i32_st   = st(i32_type);

		// Get all symbol types before entering query context
		const auto p_type         = getSymbolTypeOf("p", body_scope);
		const auto from_ref_type  = getSymbolTypeOf("from_ref", body_scope);
		const auto from_addr_type = getSymbolTypeOf("from_addr", body_scope);
		const auto from_box_type  = getSymbolTypeOf("from_box", body_scope);
		const auto c_type         = getSymbolTypeOf("c", body_scope);
		const auto m_type         = getSymbolTypeOf("m", body_scope);
		const auto pp_type        = getSymbolTypeOf("pp", body_scope);
		const auto inner_type     = getSymbolTypeOf("inner", body_scope);
		const auto deref_val_type = getSymbolTypeOf("deref_val", body_scope);
		const auto sum_type       = getSymbolTypeOf("sum", body_scope);
		const auto c_elem_type    = getSymbolTypeOf("c_elem", body_scope);

		const auto pof_direct_type = getSymbolTypeOf("pof_direct", body_scope);
		const auto pof_box_type    = getSymbolTypeOf("pof_box", body_scope);
		const auto pof_ref_type    = getSymbolTypeOf("pof_ref", body_scope);
		const auto pof_elem_type   = getSymbolTypeOf("pof_elem", body_scope);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto ptr_i32     = ctx.query<compiler::tsh::QueryPointerType>({ i32_st });
			const auto ptr_ptr_i32 = ctx.query<compiler::tsh::QueryPointerType>({ st(ptr_i32) });
			const auto manyptr_i32 = ctx.query<compiler::tsh::QueryManyPointerType>({ i32_st });
			const auto cptr_i32    = ctx.query<compiler::tsh::QueryCPointerType>({ i32_st });

			const auto ptr_i32_st     = st(ptr_i32);
			const auto ptr_ptr_i32_st = st(ptr_ptr_i32);
			const auto manyptr_i32_st = st(manyptr_i32);
			const auto cptr_i32_st    = st(cptr_i32);

			ASSERT_EQUAL(ptr_i32_st, p_type);
			ASSERT_EQUAL(ptr_i32_st, from_ref_type);
			ASSERT_EQUAL(ptr_i32_st, from_addr_type);
			ASSERT_EQUAL(ptr_i32_st, from_box_type);
			ASSERT_EQUAL(cptr_i32_st, c_type);
			ASSERT_EQUAL(manyptr_i32_st, m_type);
			ASSERT_EQUAL(ptr_ptr_i32_st, pp_type);
			ASSERT_EQUAL(ptr_i32_st, inner_type);
			ASSERT_EQUAL(i32_st, deref_val_type);
			ASSERT_EQUAL(i32_st, sum_type);
			ASSERT_EQUAL(i32_st, c_elem_type);

			// `ptrof x` is `ptr S` for the whole symbol type `S` of `x`: unlike `&x`, a `box`/`ref`
			// operand is not collapsed, the pointer addresses the box/reference itself.
			using compiler::tsh::ReferenceKind;
			ASSERT_EQUAL(ptr_i32_st, pof_direct_type);
			ASSERT_EQUAL(
				st(ctx.query<compiler::tsh::QueryPointerType>(
					{ i32_st.withReferenceKind(ReferenceKind::Box) }
				)),
				pof_box_type
			);
			ASSERT_EQUAL(
				st(ctx.query<compiler::tsh::QueryPointerType>(
					{ i32_st.withReferenceKind(ReferenceKind::Ref) }
				)),
				pof_ref_type
			);
			// Indexing a `manyptr i32` gives an `i32` place, so its address is a plain `ptr i32`.
			ASSERT_EQUAL(ptr_i32_st, pof_elem_type);
		});

		auto& function = hout.functions.at(0);
		ASSERT_EQUAL(base::StrID("test_pointers"), function->declaration->original_name);

		auto get_var_init_expr
			= [&](const base::StrID& varname) -> CRef<compiler::helios::code::Expr> {
			for (const auto& st_box: function->body->statements) {
				if (auto var_ptr
				    = dynamic_cast<const compiler::helios::code::VariableStmt*>(st_box.get())) {
					if (compiler::helios::name(var_ptr->helios_symbol) == varname)
						return var_ptr->initial_value.ref();
				}
			}
			CORE_PANIC("Variable not found in function body");
		};

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_ref"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			ASSERT_EQUAL(compiler::tsh::Kind::Pointer, cast_ptr->target_type.getType().getKind());
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_addr"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_box"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("c"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			ASSERT_EQUAL(compiler::tsh::Kind::CPointer, cast_ptr->target_type.getType().getKind());
		}

		{
			auto expr_ptr  = get_var_init_expr(base::StrID("c_elem"));
			auto index_ptr = dynamic_cast<const compiler::helios::code::IndexExpr*>(expr_ptr.get());
			ASSERT_TRUE(index_ptr != nullptr);
			ASSERT_EQUAL(
				compiler::tsh::Kind::CPointer, index_ptr->base->expression_type.getType().getKind()
			);
			ASSERT_EQUAL(i32_st, index_ptr->expression_type.getSymbolType().withMutability(Mutable));
		}

		{
			auto expr_ptr  = get_var_init_expr(base::StrID("deref_val"));
			auto deref_ptr = dynamic_cast<const compiler::helios::code::DerefExpr*>(expr_ptr.get());
			ASSERT_TRUE(deref_ptr != nullptr);
			ASSERT_EQUAL(i32_st, deref_ptr->expression_type.getSymbolType().withMutability(Mutable));
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("sum"));
			auto bin_ptr
				= dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(expr_ptr.get());
			ASSERT_TRUE(bin_ptr != nullptr);
			auto deref_lhs
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(bin_ptr->lhs.get());
			ASSERT_TRUE(deref_lhs != nullptr);
			ASSERT_EQUAL(i32_st, deref_lhs->expression_type.getSymbolType().withMutability(Mutable));
		}

		{
			auto  expr_ptr = get_var_init_expr(base::StrID("pof_direct"));
			auto* ptr_of   = dynamic_cast<const compiler::helios::code::PtrOfExpr*>(expr_ptr.get());
			ASSERT_TRUE(ptr_of != nullptr);

			// A clone is an independent node printing exactly like the original.
			auto              cloned = ptr_of->clone();
			std::stringstream orig_out, clone_out;
			ptr_of->debugPrint(orig_out);
			cloned->debugPrint(clone_out);
			ASSERT_EQUAL(orig_out.str(), clone_out.str());
			ASSERT_TRUE(orig_out.str().starts_with("ptrof("));
			ASSERT_TRUE(&(*cloned) != ptr_of);
		}
	}

	/**
	 * `copy` produces a HOUT expression yielding a copy of its source, dispatching on the source
	 * type: trivially-copyable sources are returned untouched (a byte copy needs no HOUT node),
	 * `box T` is deep-copied into a fresh allocation, and other non-trivial aggregates go through
	 * their copy constructor.
	 * A module supplies a class (`HasBox`) that owns a `box` — hence non-trivially-copyable.
	 */
	void testCopy() {
		using namespace compiler;
		using namespace compiler::helios::code;
		using namespace compiler::helios::code::shorthands;

		const auto [module, scope] = getModule(fs::File(path("test_modules/shorthands/copy")));
		const auto holder_var      = getChain("holder", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			// 1. Trivially-copyable primitive: returned as-is, no wrapping node.
			const auto trivial = s.copyValue(s.litNum(5));
			ASSERT_TRUE(dynamic_cast<const LiteralNumericExpr*>(trivial.get()) != nullptr);

			// 2. A non-trivially-copyable class: copied via a call to its copy constructor, taking
			//    a reference to the source.
			const auto class_copy = s.copyValue(s.ident(holder_var));
			ASSERT_EQUAL(class_copy->expression_type.getType().getKind(), tsh::Kind::Class);
			const auto* class_call = dynamic_cast<const CallExpr*>(class_copy.get());
			ASSERT_TRUE(class_call != nullptr);
			ASSERT_EQUAL(class_call->arguments.size(), 1UL);
			const auto* ref_arg = dynamic_cast<const RefOfExpr*>(class_call->arguments.at(0).get());
			ASSERT_TRUE(ref_arg != nullptr);

			// 3. A `box T` field is deep-copied into a fresh heap allocation (a `boxAlloc` call
			//    read back as a box), not returned as-is.
			const auto boxed_field = ctx.query<helios::QueryTypeOfSymbol>(holder_var)
			                             ->valueOrThrow()
			                             .getType()
			                             .getInterface(ctx)
			                             ->getElementsWithName(base::StrID("boxed"))
			                             .back()
			                             .getSymbol();
			const auto box_copy = s.copyValue(s.access(s.ident(holder_var), boxed_field));
			ASSERT_EQUAL(
				box_copy->expression_type.getSymbolType().getRefKind(), tsh::ReferenceKind::Box
			);
			ASSERT_TRUE(boxAllocArg(box_copy.get()) != nullptr);
		});
	}

	void testOperatorsWithPrimitives() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/std_operators")));
		ASSERT_EQUAL(std::numeric_limits<i32>::max(), getConstValueAs<i32>("MAX_I32", root_scope));

		ASSERT_EQUAL(256, getConstValueAs<i32>("V256", root_scope));

		auto              sym_v256 = getChain("V256", root_scope).back();
		std::stringstream out_v256;
		auto              tree_v256 = getExprOfConst(sym_v256);
		tree_v256->debugPrint(out_v256);
		ASSERT_TRUE(std::regex_match(
			out_v256.str(),
			std::regex{ R"(\(Symbol powi \((\d+)\)\)\(3 \+ 4 - 4 \* 16 / 5 % 7, 8\))" }
		));
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")

/**
 * @file helios_with_std_test.cpp
 * @brief HELIOS tests that need the standard library available (e.g. to resolve
 * `import core.builtins.*`). They initialize the compiler via the driver test utils, unlike
 * helios_test.cpp which builds standalone module trees without a std.
 */

#include "helios/hout/elements/stmt.hpp"
#include "helios/test_utils/helios_test_utils.hpp"
#include "helios/tsh/mutability.hpp"
#include "helios/tsh/queries/types.hpp"

#include <driver/test_utils.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>

#include "query_framework/entry/with_context_do.hpp"
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <any>

using namespace compiler::helios;
using namespace compiler::helios::test_utils;

class HeliosWithStdTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosWithStdTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBuiltinDefinitionInModuleHOUT);
		TESTER_ADD_TEST(testStrings);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("test_modules/builtins")), "builtins" }
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

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
		return std::any_cast<compiler::tsh::ClassAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return compiler::tsh::getStringType(ctx);
			})
		);
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

	void testStrings() {
		auto [module, scope] = getModule(fs::File(path("test_modules/strings")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function   = hout.functions.at(0);
		auto  fun_sym    = getChain("main", scope).back();
		auto& statements = function->body->statements;
		using namespace compiler::helios::code;

		{
			// let ab = 'a' +: b;
			const auto& prepended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(1));
			const auto  prepended_expr
				= dynamic_cast<const CallExpr*>(prepended_stmt.initial_value.get());
			const auto prepended_callee
				= dynamic_cast<IdentifierExpr*>(prepended_expr->callee.get());
			assertEqual(
				compiler::helios::name(prepended_callee->symbol),
				base::StrID("prependChar"),
				"The prepended expression should call prependChar"
			);
		}

		{
			// let bcd = b :+ 'c' :+ 'd';
			const auto& appended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(2));
			const auto  appended_expr
				= dynamic_cast<const CallExpr*>(appended_stmt.initial_value.get());
			const auto appended_callee = dynamic_cast<IdentifierExpr*>(appended_expr->callee.get());
			assertEqual(
				compiler::helios::name(appended_callee->symbol),
				base::StrID("appendChar"),
				"The appended expression should call appendChar"
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
				base::StrID("concatStrings"),
				"The concatenated expression should call concatStrings"
			);
		}

		{
			// let x = 1;
			// let y = 2;
			// let format = "Did you know that {x} plus {y} equals ({x + y})?";
			const auto& format_stmt = dynamic_cast<const VariableStmt&>(*statements.at(8));
			const auto format_expr = dynamic_cast<const CallExpr*>(format_stmt.initial_value.get());
			const auto format_callee = dynamic_cast<IdentifierExpr*>(format_expr->callee.get());
			assertEqual(
				compiler::helios::name(format_callee->symbol),
				base::StrID("concatStrings"),
				"The format string expression should call concatStrings"
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

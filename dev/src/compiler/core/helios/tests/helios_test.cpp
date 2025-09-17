#include <frontend/module_tree/queries.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_class_symbol_data.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/pst_query/code_dependency.hpp>
#include <pst_parser/test_utils/pst_test_utils.hpp>
#include <typesystem/higher/all.hpp>
#include <typesystem/higher/internal/queries.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>

#include <diagnostic/highlight_positions.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testImport);
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testError);
		TESTER_ADD_TEST(testConstants);
		TESTER_ADD_TEST(testClassSymbolData);
		TESTER_ADD_TEST(testHoutVariables);
		TESTER_ADD_TEST(testExprTree);
		TESTER_ADD_TEST(testSimpleHOUT);
		TESTER_ADD_TEST(testSingleFileModuleHOUT);
		TESTER_ADD_TEST(testModuleHOUT);
		TESTER_ADD_TEST(testDependencyHOUT);
		TESTER_ADD_TEST(testHoutVisitor);
		TESTER_ADD_TEST(testTypeOf);
		TESTER_ADD_TEST(testKeywordLiterals);
		TESTER_ADD_TEST(testFunctionParameters);
		TESTER_ADD_TEST(testExprScopes);
		TESTER_ADD_TEST(testFunctionCallExpr);
		TESTER_ADD_TEST(testFunctions);
		TESTER_ADD_TEST(testBuiltinFunctions);
		TESTER_ADD_TEST(testMangler);
		TESTER_ADD_TEST(testGlobalVariableExpressions);
		TESTER_ADD_TEST(testTypeOfConstAndVar);
		TESTER_ADD_TEST(testDebugPrint);

		// this is at the end
		// so we test all the scopes created in helios tests:
		TESTER_ADD_TEST(testScopeParentsAndDepth);
		TESTER_ADD_TEST(testScopeSymbolsConsistency);
	}

private:
	// @TODO: test_modules/aliases are not used in tests

	using enum tsh::Mutability;
	using enum tsh::IntegralAbstractType::Signedness;

	static tsh::SymbolType<> st(const tsh::AbstractType abstract_type) {
		return tsh::SymbolType{
			abstract_type,
			tsh::ReferenceKind::Direct,
			Mutable,
		};
	}

	void testConstants() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/constants")));

		ASSERT_EQUAL(1'107, getConstValueAs<i64>("M", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("N.X", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("A", root_scope));
		ASSERT_EQUAL(-3, getConstValueAs<i64>("B", root_scope));
		ASSERT_EQUAL(-1, getConstValueAs<i64>("D", root_scope));
		ASSERT_EQUAL(6, getConstValueAs<i64>("E", root_scope));
		ASSERT_EQUAL(std::numeric_limits<i32>::max(), getConstValueAs<i64>("MAX_I32", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("H2", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("T0", root_scope));
		ASSERT_EQUAL(2, getConstValueAs<i64>("T1", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("T2", root_scope));
		ASSERT_EQUAL(30, getConstValueAs<i64>("F", root_scope));

		ASSERT_EQUAL(true, getConstValueAs<bool>("BOOL_TRUE", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("BOOL_FALSE", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("LOGIC_AND", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("LOGIC_OR", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("TRUE_COMPARISON", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("FALSE_COMPARISON", root_scope));

		ASSERT_EQUAL(42, getConstValueAs<i64>("VM_SIMPLE_CALL", root_scope));
		ASSERT_EQUAL(1'129, getConstValueAs<i64>("VM_SIMPLE_CALL_2", root_scope));
		ASSERT_EQUAL(55, getConstValueAs<i64>("FIB_10", root_scope));
		ASSERT_EQUAL(58, getConstValueAs<i64>("COMPLEX_VM_CALL", root_scope));
		ASSERT_EQUAL(37, getConstValueAs<i64>("COMPLEX_VM_CALL_2", root_scope));
	}

	void testClassSymbolData() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/classes")));

		const auto first_class = getChain("FirstClassEver", root_scope).back();
		const auto first_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(first_class)->valueOrThrow();
		const auto first_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_class)
		          ->valueOrThrow()
		          .getType();

		ASSERT_EQUAL(2, first_class_info.members.size());
		ASSERT_EQUAL(2, first_class_info.methods.size());
		ASSERT_EQUAL(1, first_class_info.constructors.size());
		ASSERT_TRUE(first_class_info.destructor.has_value());
		ASSERT_TRUE(not first_class_info.base.has_value());
		ASSERT_EQUAL(0, first_class_info.implements.size());
		ASSERT_EQUAL("FirstClassEver", first_class_info.name);

		const auto second_class = getChain("SecondClass", root_scope).back();
		auto       second_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(second_class)->valueOrThrow();

		ASSERT_EQUAL(0, second_class_info.members.size());
		ASSERT_EQUAL(0, second_class_info.methods.size());
		ASSERT_EQUAL(0, second_class_info.constructors.size());
		ASSERT_TRUE(not second_class_info.destructor.has_value());
		ASSERT_TRUE(second_class_info.base.has_value());
		ASSERT_EQUAL(first_class_abstract_type, second_class_info.base);
		ASSERT_EQUAL("SecondClass", second_class_info.name);
	}

	void testTypeOf() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/types")));

		const auto int16_type = query::entryPoint<tsh::QueryIntegralType>({ 16, Signed });
		const auto int32_type = query::entryPoint<tsh::QueryIntegralType>({ 32, Signed });
		const auto f16_type   = query::entryPoint<tsh::QueryFloatType>(16);
		const auto f32_type   = query::entryPoint<tsh::QueryFloatType>(32);
		const auto bool_type  = query::entryPoint<tsh::QueryBoolType>({});
		const auto meta_type  = query::entryPoint<tsh::QueryMetaType>({});
		const auto str_type   = query::entryPoint<tsh::QueryStringType>({});

		ASSERT_EQUAL(int32_type, getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(f32_type, getTypeOf("SimpleFloat", root_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("SimpleBool", root_scope));
		ASSERT_EQUAL(str_type, getTypeOf("SimpleString", root_scope));

		const auto tuple_int_int = getTypeOf("TupleII", root_scope);
		const auto tuple_int_int_abstract_type
			= query::entryPoint<tsh::QueryTupleType>({ { st(int32_type), st(int32_type) } });
		ASSERT_EQUAL(tuple_int_int, tuple_int_int_abstract_type);

		const auto first_variant = getTypeOf("first_variant", root_scope);
		const auto first_variant_abstract_type
			= query::entryPoint<tsh::QueryVariantType>({ { st(int32_type), st(f32_type) } });
		ASSERT_EQUAL(first_variant, first_variant_abstract_type);

		const auto second_variant               = getTypeOf("second_variant", root_scope);
		const auto second_variant_abstract_type = query::entryPoint<tsh::QueryVariantType>(
			{ { st(int32_type), st(f32_type), st(bool_type) } }
		);
		ASSERT_EQUAL(second_variant, second_variant_abstract_type);

		const auto weird_variant = getTypeOf("weird_variant", root_scope);

		const auto class_a = getTypeFromDefinition("A", root_scope);
		const auto class_b = getTypeFromDefinition("B", root_scope);
		const auto class_c = getTypeFromDefinition("C", root_scope);

		auto right_tuple = query::entryPoint<tsh::QueryTupleType>({ {
			class_a,
			st(query::entryPoint<tsh::QueryVariantType>({ { class_b, class_c } })),
		} });

		const auto weird_variant_type
			= query::entryPoint<tsh::QueryVariantType>({ { class_a, st(right_tuple) } });

		ASSERT_EQUAL(weird_variant, weird_variant_type);

		ASSERT_EQUAL(meta_type, getTypeOf("T", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("A", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("B", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("C", root_scope));

		const auto tuple_ii_ff = getTypeOf("TupleIIFF", root_scope);

		const auto tuple_f16_f32
			= query::entryPoint<tsh::QueryTupleType>({ { st(f16_type), st(f32_type) } });
		const auto tuple_i16_i32
			= query::entryPoint<tsh::QueryTupleType>({ { st(int16_type), st(int32_type) } });

		const auto tuple_ii_ff_abstract_type
			= query::entryPoint<tsh::QueryTupleType>({ { st(tuple_i16_i32), st(tuple_f16_f32) } });

		ASSERT_EQUAL(tuple_ii_ff, tuple_ii_ff_abstract_type);
	}

	void testEdgeEvals() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/edge_evals")));
		ASSERT_EQUAL(1, getConstValueAs<i64>("M1", root_scope));
		ASSERT_EQUAL(6, getConstValueAs<i64>("M2", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("O1", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("O2", root_scope));
		// These do not work anymore.
		// ASSERT_EQUAL(7, getValue("O3", root_scope));
		// ASSERT_EQUAL(7, getValue("O4", root_scope));
	}

	void testSimpleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_simple_test")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout->functions.size(), 3);
		ASSERT_EQUAL(hout->glob_data.size(), 3);

		// just for cov and to see if it does not throw:
		[[maybe_unused]] auto hout_debug_print = hout->debugPrint();
	}

	void testSingleFileModuleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/simple_scopes")));

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module);

		unsigned long functions = 0;
		unsigned long glob_data = 0;

		for (const auto& hout: houts) {
			functions += hout.functions.size();
			glob_data += hout.glob_data.size();
		}

		ASSERT_EQUAL(functions, 3);
		ASSERT_EQUAL(glob_data, 5);
	}

	void testModuleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_module")));

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module);

		unsigned long functions = 0;
		unsigned long glob_data = 0;

		for (const auto& hout: houts) {
			functions += hout.functions.size();
			glob_data += hout.glob_data.size();
		}

		ASSERT_EQUAL(functions, 1);
		ASSERT_EQUAL(glob_data, 5);
	}

	void testDependencyHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_simple_test")));

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module);

		for (const auto& hout: houts) {
			for (const auto& fun: hout.functions) {
				std::cerr << fun.original_name.str() << " i dependent on\n";
				auto positions = pst::queryPositionDependencies<compiler::helios::QueryCodeOFFun>(
					fun.original_symbol
				);

				auto tokens = pst::queryTokenDependencies<compiler::helios::QueryCodeOFFun>(
					fun.original_symbol
				);

				printer::PrinterOStream str;
				dia::printHighlightedPositions(str, positions);

				printer::StreamPrinter p;
				p.print(str.getContents());
			}
		}
	}

	void testHoutVisitor() {
		auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::File(path("test_modules/visitor_test_module"))
		);

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout->functions.size(), 1);

		auto the_function = hout->functions.at(0);

		auto& stmt_list = the_function.content.body->statements;
		ASSERT_EQUAL(stmt_list.size(), 6);

		using namespace compiler::helios::code;

		struct StmtVisitor: public HoutStmtVisitorPanicky {
			usize expr_stmt_count        = 0;
			usize return_stmt_count      = 0;
			usize void_return_stmt_count = 0;
			usize if_stmt_count          = 0;
			usize while_stmt_count       = 0;

			void visitExprStmt(const ExprStmt&) override { expr_stmt_count++; }

			void visitReturnStmt(const ReturnStmt&) override { return_stmt_count++; }

			void visitVoidReturnStmt(const VoidReturnStmt&) override { void_return_stmt_count++; }

			void visitIfStmt(const IfStmt&) override { if_stmt_count++; }

			void visitWhileStmt(const WhileStmt&) override { while_stmt_count++; }
		};

		{
			StmtVisitor visitor;
			stmt_list.at(0)->acceptVisitor(visitor);
			stmt_list.at(1)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_stmt_count, 2);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(2)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.return_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(3)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.void_return_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(4)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.if_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(5)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.while_stmt_count, 1);
		}

		struct ExprVisitor: public HoutExprVisitorPanicky {
			usize const_int_count = 0;
			usize ident_count     = 0;

			void visitLiteralIntExpr(const LiteralIntExpr&) override { const_int_count++; }

			void visitIdentifierExpr(const IdentifierExpr&) override { ident_count++; }

			void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override { ident_count++; }

			void visitParenthesisExpr(const ParenthesisExpr&) override { ident_count++; }
		};

		struct ExprVisitorRunner: public HoutStmtVisitorPanicky {
			ExprVisitor expr_visitor;

			void visitExprStmt(const ExprStmt& expr) override {
				expr.expr->acceptVisitor(expr_visitor);
			}
		};

		{
			ExprVisitorRunner visitor;
			stmt_list.at(0)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_visitor.const_int_count, 1);
		}
		{
			ExprVisitorRunner visitor;
			stmt_list.at(1)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_visitor.ident_count, 1);
		}
	}

	void testImport() {
		auto [module, _] = getModule(fs::File(path("test_modules/import_tests")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		auto test_value = [&](auto str, i64 exp_val) {
			auto name = base::StrID(str);
			for (auto& gb: hout->glob_data) {
				if (gb.original_name == name) {
					if (std::holds_alternative<compiler::helios::HOUTGlobalConst>(gb.value)) {
						auto ctv = std::get<compiler::helios::HOUTGlobalConst>(gb.value).value;
						auto val = ctv.asI64();
						if (!val.has_value()) {
							this->fail(base::strConcat(
								"Got a constant with a different type than expected", name.strView()
							));
						}
						ASSERT_EQUAL(exp_val, val);
						return;
					} else {
						this->fail(base::strConcat("Expected constant but found: ", name.strView()));
					}
				}
			}
			this->fail(base::strConcat("No constant of name: ", name.strView()));
		};

		test_value("sm1_v", 123'123);
		test_value("sm11_v", 7'812'313);
		test_value("it_through_alias", 19'923);
		test_value("sm1_through_sm11", 123'123);
		test_value("sm2_v", 777'666);
		test_value("cyclic_final", 6);
	}

	void testExprTree() {
		auto symbol_name = [](const char* name, auto&& symbol) {
			return base::strConcat("(Symbol ", name, " (", symbol.queryUnstablePerfectHash(), "))");
		};

		auto [_, root_scope] = getModule(fs::File(path("test_modules/expressions")));

		ASSERT_EQUAL(1, getConstValueAs<i64>("V1", root_scope));
		auto              sym_v1  = getChain("V1", root_scope).back();
		auto              tree_v1 = getExprOfConst(sym_v1);
		std::stringstream out_v1;
		tree_v1->debugPrint(out_v1);

		ASSERT_EQUAL(-1, getConstValueAs<i64>("VM1", root_scope));
		auto              sym_vm1  = getChain("VM1", root_scope).back();
		auto              tree_vm1 = getExprOfConst(sym_vm1);
		std::stringstream out_vm1;
		tree_vm1->debugPrint(out_vm1);

		ASSERT_EQUAL(256, getConstValueAs<i64>("V256", root_scope));

		auto              sym_v256 = getChain("V256", root_scope).back();
		std::stringstream out_v256;
		auto              tree_v256 = getExprOfConst(sym_v256);
		tree_v256->debugPrint(out_v256);
		ASSERT_EQUAL("(3+4-4*16/5%7)**8", out_v256.str());

		ASSERT_EQUAL(12, getConstValueAs<i64>("V12", root_scope));
		auto              sym_v12  = getChain("V12", root_scope).back();
		auto              tree_v12 = getExprOfConst(sym_v12);
		std::stringstream out_v12;
		tree_v12->debugPrint(out_v12);

		auto sym_v3      = getChain("N.V3", root_scope).back();
		auto sym_v3_repr = symbol_name("V3", sym_v3);
		ASSERT_EQUAL(
			base::strConcat(sym_v3_repr, "+", sym_v3_repr, "*", sym_v3_repr), out_v12.str()
		);

		ASSERT_EQUAL(false, getConstValueAs<bool>("CMP", root_scope));
		auto              get_cmp  = getChain("CMP", root_scope).back();
		auto              expr_cmp = getExprOfConst(get_cmp);
		std::stringstream out_cmp;
		expr_cmp->debugPrint(out_cmp);
		ASSERT_EQUAL_PRINT(
			(base::strConcat(
				symbol_name("V1", sym_v1), "<3<=4==5!=6>=7>", symbol_name("VM1", sym_vm1)
			)),
			out_cmp.str()
		);

		auto get_str  = getChain("STR", root_scope).back();
		auto expr_str = getExprOfConst(get_str);
		Ref  expr_str_casted
			= dynamic_cast<const compiler::helios::code::LiteralStringExpr*>(&*expr_str);
		ASSERT_EQUAL("quack", expr_str_casted->value.str());

		auto              sym_vref  = getChain("VREF", root_scope).back();
		auto              tree_vref = getExprOfConst(sym_vref);
		std::stringstream out_vref;
		tree_vref->debugPrint(out_vref);
		const auto int32_type    = query::entryPoint<tsh::QueryIntegralType>({ 32, Signed });
		const auto int32ref_type = st(int32_type).withReferenceKind(tsh::ReferenceKind::Ref);
		const auto vref_type     = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vref);
		ASSERT_EQUAL(int32ref_type, vref_type->valueOrThrow());

		auto              sym_vbox  = getChain("VBOX", root_scope).back();
		auto              tree_vbox = getExprOfConst(sym_vbox);
		std::stringstream out_vbox;
		tree_vbox->debugPrint(out_vbox);
		const auto f16_type    = query::entryPoint<tsh::QueryFloatType>(16);
		const auto f16box_type = st(f16_type).withReferenceKind(tsh::ReferenceKind::Box);
		const auto vbox_type   = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vbox);
		ASSERT_EQUAL(f16box_type, vbox_type->valueOrThrow());
	}

	void testError() {
		using namespace compiler::helios;

		auto [_, root_scope] = getModule(fs::File(path("test_modules/error_generating/bad_expr")));

		try {
			getConstValueAs<i64>("InvalidExpr", root_scope);
			CORE_PANIC("Should throw.");
		} catch (base::NotYetImplemented& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			getConstValueAs<i64>("InvalidSym", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			getConstValueAs<i64>("C", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}

		// This fails on the HOUT creation level instead of during the evaluation.
		// @TODO: #1287 write a test that checks failing compile-time evaluation of comparison chain.
		try {
			getConstValueAs<bool>("InvalidCompMiddle", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			getConstValueAs<bool>("InvalidCompFirst", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}
	}

	void testHoutVariables() {
		auto [module, _] = getModule(fs::File(path("test_modules/variables")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout->functions.size(), 1);

		auto& function = hout->functions.at(0);

		ASSERT_EQUAL(function.original_name, "foo");

		// note that alias should not be included here:
		ASSERT_EQUAL(function.content.body->statements.size(), 10);

		auto& statements = function.content.body->statements;

		auto get_var_block = [&](usize i, auto&& code_block) -> decltype(auto) {
			return dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*code_block.statements.at(i)
			);
		};

		auto get_var_ref
			= [&](usize i) -> decltype(auto) { return get_var_block(i, *function.content.body); };


		auto i32_type = query::entryPoint<tsh::QueryIntegralType>(32);
		auto f32_type = query::entryPoint<tsh::QueryFloatType>(32);
		auto i32_or_f32
			= query::entryPoint<tsh::QueryVariantType>({ { st(i32_type), st(f32_type) } });


		{
			auto& var = get_var_ref(0);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "a");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			auto& var = get_var_ref(1);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "b");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			auto& var = get_var_ref(2);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "c");
			ASSERT_EQUAL(var.type, st(i32_or_f32));
		}

		{
			auto& var = get_var_ref(3);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "d");
			ASSERT_EQUAL(var.type.getType().getKind(), tsh::Kind::Class);
		}

		{
			auto& if_stmt = dynamic_cast<const compiler::helios::code::IfStmt&>(*statements.at(4));
			{
				auto& var1 = get_var_block(0, if_stmt.then_body);
				ASSERT_EQUAL(compiler::helios::name(var1.helios_symbol), "x");
				ASSERT_EQUAL(var1.type, st(i32_type));

				auto& var2 = get_var_block(1, if_stmt.then_body);
				ASSERT_EQUAL(compiler::helios::name(var2.helios_symbol), "y");
				ASSERT_EQUAL(var2.type, st(i32_or_f32));
			}
			{
				auto& var = get_var_block(0, if_stmt.else_body);
				ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "z");
				ASSERT_EQUAL(var.type, st(i32_type));
			}
		}

		{
			auto& while_stmt
				= dynamic_cast<const compiler::helios::code::WhileStmt&>(*statements.at(5));
			auto& var = get_var_block(0, while_stmt.body);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "a");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			auto& var = get_var_ref(6);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "e");
			ASSERT_EQUAL(var.type.getType().getKind(), tsh::Kind::Class);
		}

		// debug print test just for cov and to see if it does not throw:
		[[maybe_unused]] auto debug_print_out = hout->debugPrint();
	}

	void testKeywordLiterals() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/keyword_literals")));

		auto i8_type   = query::entryPoint<tsh::QueryIntegralType>({ 8, Signed });
		auto i16_type  = query::entryPoint<tsh::QueryIntegralType>({ 16, Signed });
		auto i32_type  = query::entryPoint<tsh::QueryIntegralType>({ 32, Signed });
		auto i64_type  = query::entryPoint<tsh::QueryIntegralType>({ 64, Signed });
		auto i128_type = query::entryPoint<tsh::QueryIntegralType>({ 128, Signed });

		auto u8_type   = query::entryPoint<tsh::QueryIntegralType>({ 8, Unsigned });
		auto u16_type  = query::entryPoint<tsh::QueryIntegralType>({ 16, Unsigned });
		auto u32_type  = query::entryPoint<tsh::QueryIntegralType>({ 32, Unsigned });
		auto u64_type  = query::entryPoint<tsh::QueryIntegralType>({ 64, Unsigned });
		auto u128_type = query::entryPoint<tsh::QueryIntegralType>({ 128, Unsigned });

		auto f16_type = query::entryPoint<tsh::QueryFloatType>(16);
		auto f32_type = query::entryPoint<tsh::QueryFloatType>(32);
		auto f64_type = query::entryPoint<tsh::QueryFloatType>(64);

		auto f80_type  = query::entryPoint<tsh::QueryFloatType>(80);
		auto f128_type = query::entryPoint<tsh::QueryFloatType>(128);

		auto char_type = query::entryPoint<tsh::QueryCharType>({});

		auto bool_type = query::entryPoint<tsh::QueryBoolType>({});

		auto str_type = query::entryPoint<tsh::QueryStringType>({});

		// a simple way to get function scope through hout:
		auto foo            = getChain("foo", top_scope).back();
		auto foo_body_scope = getFunctionBodyScope(foo);

		// variable types:

		ASSERT_EQUAL(i8_type, getTypeOf("v_i8", foo_body_scope));
		ASSERT_EQUAL(i16_type, getTypeOf("v_i16", foo_body_scope));
		ASSERT_EQUAL(i32_type, getTypeOf("v_i32", foo_body_scope));
		ASSERT_EQUAL(i64_type, getTypeOf("v_i64", foo_body_scope));
		ASSERT_EQUAL(i128_type, getTypeOf("v_i128", foo_body_scope));

		ASSERT_EQUAL(u8_type, getTypeOf("v_u8", foo_body_scope));
		ASSERT_EQUAL(u16_type, getTypeOf("v_u16", foo_body_scope));
		ASSERT_EQUAL(u32_type, getTypeOf("v_u32", foo_body_scope));
		ASSERT_EQUAL(u64_type, getTypeOf("v_u64", foo_body_scope));
		ASSERT_EQUAL(u128_type, getTypeOf("v_u128", foo_body_scope));

		ASSERT_EQUAL(f16_type, getTypeOf("v_f16", foo_body_scope));
		ASSERT_EQUAL(f32_type, getTypeOf("v_f32", foo_body_scope));
		ASSERT_EQUAL(f64_type, getTypeOf("v_f64", foo_body_scope));
		ASSERT_EQUAL(f80_type, getTypeOf("v_f80", foo_body_scope));
		ASSERT_EQUAL(f128_type, getTypeOf("v_f128", foo_body_scope));

		ASSERT_EQUAL(char_type, getTypeOf("v_char", foo_body_scope));

		ASSERT_EQUAL(bool_type, getTypeOf("v_bool_t", foo_body_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("v_bool_f", foo_body_scope));

		ASSERT_EQUAL(str_type, getTypeOf("v_str", foo_body_scope));

		// true, false literals:
		auto true_expr  = getExprOfVariable(getChain("v_bool_t", foo_body_scope).back());
		auto false_expr = getExprOfVariable(getChain("v_bool_f", foo_body_scope).back());

		auto true_expr_casted
			= dynamic_cast<const compiler::helios::code::LiteralBoolExpr*>(&*true_expr);
		auto false_expr_casted
			= dynamic_cast<const compiler::helios::code::LiteralBoolExpr*>(&*false_expr);

		ASSERT_TRUE(true_expr_casted != nullptr);
		ASSERT_TRUE(false_expr_casted != nullptr);

		ASSERT_EQUAL(true_expr_casted->value, true);
		ASSERT_EQUAL(false_expr_casted->value, false);
	}

	void testFunctionParameters() {
		auto [module, _] = getModule(fs::File(path("test_modules/parameters")));

		const auto int32_type = query::entryPoint<tsh::QueryIntegralType>({ 32, Signed });
		const auto int64_type = query::entryPoint<tsh::QueryIntegralType>({ 64, Signed });

		query::utils::withContextDo([&](query::Context& ctx) {
			auto hout = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			ASSERT_EQUAL(hout->functions.size(), 2);
			{
				auto function = hout->functions.at(0);
				ASSERT_EQUAL(function.original_name, "foo");

				auto& a_param = function.content.parameters->at(0);
				ASSERT_EQUAL("a", a_param.name);
				ASSERT_EQUAL(st(int32_type), a_param.type);
				assertTrue(a_param.initial_value.empty(), "No initial value expected");

				// get "a" thru return:
				ASSERT_EQUAL(function.content.body->statements.size(), 1);

				auto ret_stmt = function.content.body->statements.at(0).ref();
				auto ret_stmt_casted
					= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
				assertTrue(ret_stmt_casted != nullptr, "Return statement expected");

				auto ret_expr = ret_stmt_casted->value.ref();
				auto ret_expr_casted
					= dynamic_cast<const compiler::helios::code::IdentifierExpr*>(&*ret_expr);
				assertTrue(ret_expr_casted != nullptr, "Identifier expression expected");

				auto a_sym  = ret_expr_casted->symbol;
				auto a_type = ret_expr_casted->expression_type;

				ASSERT_EQUAL(int32_type, a_type.getType());
				ASSERT_EQUAL(
					st(int32_type),
					ctx.query<compiler::helios::QueryTypeOfSymbol>({ a_sym })->value()
				);

				ASSERT_EQUAL(a_sym, a_param.helios_symbol);
			}

			{
				auto function = hout->functions.at(1);
				ASSERT_EQUAL(function.original_name, "bar");
				auto& abc_param    = function.content.parameters->at(0);
				auto& second_param = function.content.parameters->at(1);

				ASSERT_EQUAL("abc", abc_param.name);
				ASSERT_EQUAL("second", second_param.name);

				ASSERT_EQUAL(abc_param.type, st(int32_type));
				ASSERT_EQUAL(second_param.type, st(int64_type));

				assertTrue(abc_param.initial_value.has_value(), "Initial value expected");
				assertTrue(second_param.initial_value.empty(), "No initial value expected");
			}
		});
	}

	void testExprScopes() {
		auto [module, _] = getModule(fs::File(path("test_modules/expr_scopes")));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto main_file = ctx.query<compiler::frontend::QueryMainSourceFile>({ module });
			auto pst       = ctx.query<compiler::frontend::QueryFilePST>({ main_file });

			auto test_expr = [&](pst::AccessLocked<pst::ExprHolder> expr) {
				auto unlocked = expr.unlock(ctx);
				ASSERT_TRUE(unlocked->isTopLevel());
				auto expected_scope = ctx.query<compiler::helios::QueryPrimaryCodeScopeFor>(expr);

				// this can't be auto because of recursive lambda
				std::function<void(pst::AccessLocked<pst::LangElement>)> sub_test_expr
					= [&](pst::AccessLocked<pst::LangElement> inner_expr) {
						  auto inner_scope
							  = ctx.query<compiler::helios::QueryPrimaryCodeScopeFor>(inner_expr);
						  ASSERT_EQUAL(expected_scope, inner_scope);

						  for (auto sub_inner: inner_expr.unlock(ctx)->viewSubElements()) {
							  variant_match(sub_inner) {
								  variant_case(pst::LangElement::Child, sub_expr) {
									  sub_test_expr(sub_expr);
								  }
								  variant_default {}
							  }
						  }
					  };

				sub_test_expr(expr);
			};

			auto all_expr_holders
				= pst::viewAllSubTreeElementsFillter<pst::ExprHolder>(pst->getRootElement());

			// We test that each expr_holder and all its sub expressions
			// have the same scope as their "top expr_holder"
			for (auto expr_locked: all_expr_holders) {
				auto expr_holder = expr_locked.unlock(ctx);
				if (expr_holder->isTopLevel()) {
					// test all sub elements:
					test_expr(expr_holder);
				} else {
					// test that element has a top-expr parent
					auto element = expr_holder->getParent().value().unlock(ctx);

					while (true) {
						if (auto holder = element.dynamicCast<pst::ExprHolder>()) {
							if (holder.value()->isTopLevel()) {
								// OK, we found parent
								break;
							}
						}
						// if parent doesn't exist, we will
						// hit panic here at some point:
						element = element->getParent().value().unlock(ctx);
					}
				}
			}
		});
	}

	void testFunctionCallExpr() {
		auto [module, scope] = getModule(fs::File(path("test_modules/function_calls")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);
		ASSERT_EQUAL(2, hout->functions.size());
		std::cerr << hout->debugPrint() << '\n';
		auto function = hout->functions.at(1);
		ASSERT_EQUAL(function.original_name, "foo");
		auto variable = dynamic_cast<const compiler::helios::code::VariableStmt*>(
			function.content.body->statements.at(0).ref().get()
		);
		ASSERT_TRUE(variable != nullptr);
		auto call_expr = dynamic_cast<const compiler::helios::code::CallExpr*>(
			variable->initial_value->ref().get()
		);
		ASSERT_TRUE(call_expr != nullptr);
		auto square_symbol = getChain("square", scope).back();
		ASSERT_EQUAL(
			square_symbol, compiler::helios::getIdentifierExprSymID(call_expr->callee.ref()).value()
		);
	}

	void testFunctions() {
		auto [module, scope] = getModule(fs::File(path("test_modules/functions")));
		auto hout            = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		// modify it as needed:
		ASSERT_EQUAL(1, hout->functions.size());

		for (auto& function: hout->functions) {
			if (function.original_name == base::StrID("stmtBody1")) {
				ASSERT_EQUAL(1, function.content.body->statements.size());
				auto stmt        = function.content.body->statements.at(0).ref();
				Ref  stmt_casted = dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*stmt);
				Ref  ret_expr    = stmt_casted->value.ref();
				Ref  ret_expr_casted
					= dynamic_cast<const compiler::helios::code::LiteralIntExpr*>(&*ret_expr);
				ASSERT_EQUAL(1, ret_expr_casted->value);
			}
		}
	}

	void testBuiltinFunctions() {
		auto [module, scope] = getModule(fs::File(path("test_modules/builtins")));
		auto hout            = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);
		ASSERT_EQUAL(1, hout->functions.size());

		auto function = hout->functions.at(0);
		ASSERT_EQUAL(function.original_name, "main");

		Ref variable_stmt = dynamic_cast<const compiler::helios::code::VariableStmt*>(
			function.content.body->statements.at(0).ref().get()
		);
		Ref call_expr_1 = dynamic_cast<const compiler::helios::code::CallExpr*>(
			variable_stmt->initial_value->ref().get()
		);
		auto call_expr_1_callee
			= compiler::helios::getIdentifierExprSymID(call_expr_1->callee.ref()).value();
		ASSERT_EQUAL(compiler::helios::SymbolKind::BuiltinFunction, kind(call_expr_1_callee));
		ASSERT_EQUAL(base::StrID("builtin_input_i64"), compiler::helios::name(call_expr_1_callee));

		Ref expr_stmt = dynamic_cast<const compiler::helios::code::ExprStmt*>(
			function.content.body->statements.at(1).ref().get()
		);
		Ref  call_expr_2 = dynamic_cast<const compiler::helios::code::CallExpr*>(&*expr_stmt->expr);
		auto call_expr_2_callee
			= compiler::helios::getIdentifierExprSymID(call_expr_2->callee.ref()).value();
		ASSERT_EQUAL(compiler::helios::SymbolKind::BuiltinFunction, kind(call_expr_2_callee));
		ASSERT_EQUAL(base::StrID("builtin_output_i64"), compiler::helios::name(call_expr_2_callee));
	}

	void testScopeParentsAndDepth() {
		auto all_scopes = compiler::helios::getAllHeliosScopes();
		message(base::strConcat("Scope count: ", all_scopes.size()));
		for (auto scope: all_scopes) {
			auto depth = scopeDepth(scope);
			while (depth != 0) {
				scope = parent(scope).value();
				ASSERT_TRUE(depth > 0);
				ASSERT_EQUAL(depth - 1, scopeDepth(scope));
				depth = scopeDepth(scope);

				// it is just for cov mostly

				// we redirect cerr to a stringstream to avoid printing a lot of stuff to console
				// here:
				std::cerr.flush();
				std::stringstream buffer;
				std::streambuf*   org_buffer = std::cerr.rdbuf(buffer.rdbuf());
				defer(std::cerr.rdbuf(org_buffer));

				// @TODO: make it not print to cerr, but to ostream or string:
				scope.debugPrintScopeAndParents();

				std::cerr.flush();
			}
			assertTrue(parent(scope).empty(), "Scope at depth 0 can't have a parent");
		}
	}

	/**
	 * This checks for all symbols that if a given
	 * symbol `s` is in the scope `N`, then it is also in the
	 * output of QuerySymbolsInScope(N).
	 */
	void testScopeSymbolsConsistency() {
		auto all_symbols = compiler::helios::getAllHeliosSymbols();

		// this is quadratic in theory, if it ever get too slow,
		// we can optimize it with some maps.
		for (auto symbol: all_symbols) {
			auto maybe_scope = compiler::helios::maybeScope(symbol);
			if (maybe_scope.empty()) continue;
			auto scope            = maybe_scope.value();
			auto symbols_in_scope = query::entryPoint<compiler::helios::QuerySymbolsInScope>(scope);

			auto found = false;
			for (auto s: *symbols_in_scope) {
				if (s == symbol) {
					found = true;
					break;
				}
			}
			assertTrue(found, "Symbol was not fount in its scope");
		}
	}

	void testMangler() {
		auto [module, _] = getModule(fs::File(path("test_modules/mangling")));
		auto hout_unit   = query::entryPoint<compiler::helios::QueryModuleHOUT>(module);

		auto find_function = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                     ) -> base::Optional<compiler::helios::HOUTFunction> {
			for (const auto& fun: unit.functions)
				if (fun.original_name == name) return fun;
			fail(base::strConcat("Function ", name.strView(), " not found"));
			return {};
		};

		auto find_global = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                   ) -> base::Optional<compiler::helios::HOUTGlobalData> {
			for (const auto& glob: unit.glob_data)
				if (glob.original_name == name) return glob;
			assertTrue(false, base::strConcat("Global ", name.strView(), " not found"));
			return {};
		};

		auto goo = find_function(hout_unit, base::StrID("goooo")).value();
		std::cerr << "\nFunction name: " << goo.original_name.strView() << '\n';
		auto mangled_goo = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ goo.original_symbol, 123, "metadata_v123" }
		);
		std::cerr << "Mangled symbol: " << mangled_goo.strView() << '\n';

		auto glob = find_global(hout_unit, base::StrID("B")).value();
		std::cerr << "\nGlobal Variable name: " << glob.original_name.strView() << '\n';
		std::cerr << "Expression: ";
		std::get<compiler::helios::HOUTGlobalVariable>(glob.value)
			.initial_value.get()
			->ref()
			->debugPrint(std::cerr);
		std::cerr << '\n';
		auto mangled_glob = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ glob.helios_symbol, 321, "metadata_v321" }
		);
		std::cerr << "Mangled symbol: " << mangled_glob.strView() << '\n';

		auto g_const = find_global(hout_unit, base::StrID("Cnst")).value();
		std::cerr << "\nConst name: " << g_const.original_name.strView() << '\n';
		auto mangled_g_const = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ g_const.helios_symbol, 321, "metadata_v321" }
		);
		std::cerr << "Mangled symbol: " << mangled_g_const.strView() << '\n';

		auto sub_module    = getModule(fs::File(path("test_modules/mangling/sub")));
		auto sub_hout_unit = query::entryPoint<compiler::helios::QueryModuleHOUT>(sub_module.first);

		auto sub_fun = find_function(sub_hout_unit, base::StrID("subFun")).value();
		std::cerr << "\nSub function name: " << sub_fun.original_name.strView() << '\n';
		auto mangled_sub_fun = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ sub_fun.original_symbol, 5, "metadata_v5" }
		);
		std::cerr << "Mangled symbol: " << mangled_sub_fun.strView() << '\n';

		auto sub_cnst = find_global(sub_hout_unit, base::StrID("subConst")).value();
		std::cerr << "\nSub constant name: " << sub_cnst.original_name.strView() << '\n';
		auto mangled_sub_cnst = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ sub_cnst.helios_symbol, 5, "metadata_v5" }
		);
		std::cerr << "Mangled symbol: " << mangled_sub_cnst.strView() << '\n';

		ASSERT_EQUAL("_Q1Y_M8manglingN4Mspc3Ooo5gooooEFi32i32f64E$metadata_v123", mangled_goo.str());
		ASSERT_EQUAL("_Q5a_M8manglingN5Nmspc1BE$metadata_v321", mangled_glob.str());

		ASSERT_EQUAL("_Q5a_M8manglingN4Mspc3Ooo4CnstE$metadata_v321", mangled_g_const.str());

		ASSERT_EQUAL("_Q4_M3subN5inSub6subFunEFi32E$metadata_v5", mangled_sub_fun.str());
		ASSERT_EQUAL("_Q4_M3subN5inSub8subConstE$metadata_v5", mangled_sub_cnst.str());
	}

	void testGlobalVariableExpressions() {
		auto find_function = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                     ) -> base::Optional<compiler::helios::HOUTFunction> {
			for (const auto& fun: unit.functions)
				if (fun.original_name == name) return fun;
			fail(base::strConcat("Function ", name.strView(), " not found"));
			return {};
		};

		auto find_global = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                   ) -> base::Optional<compiler::helios::HOUTGlobalData> {
			for (const auto& glob: unit.glob_data)
				if (glob.original_name == name) return glob;
			assertTrue(false, base::strConcat("Global ", name.strView(), " not found"));
			return {};
		};

		{  // General global variable checks.

			auto [module, _] = getModule(fs::File(path("test_modules/global_variables/general")));
			auto hout_unit   = query::entryPoint<compiler::helios::QueryModuleHOUT>(module);

			ASSERT_EQUAL(hout_unit.glob_data.size(), 3);

			auto glob1 = find_global(hout_unit, base::StrID("B")).value();
			auto glob2 = find_global(hout_unit, base::StrID("XB")).value();

			Ref<const compiler::helios::code::Expr> expr1
				= std::get<compiler::helios::HOUTGlobalVariable>(glob1.value)
			          .initial_value.get()
			          ->ref();
			Ref<const compiler::helios::code::Expr> expr2
				= std::get<compiler::helios::HOUTGlobalVariable>(glob2.value)
			          .initial_value.get()
			          ->ref();

			ASSERT_EQUAL(
				compiler::helios::code::BuiltinBinary::IntegerAdd,
				dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(&*expr1)->operation
			);
			ASSERT_EQUAL(
				compiler::helios::getIdentifierExprSymID(
					dynamic_cast<const compiler::helios::code::CallExpr*>(&*expr2)->callee.ref()
				)
					.value(),
				find_function(hout_unit, base::StrID("foooo")).value().original_symbol
			);
			expr1->debugPrint(std::cerr);
			std::cerr << '\n';
		}

		{  // Global variable detections check (isGlobalVar function).
			auto [module, _] = getModule(fs::File(path("test_modules/global_variables/detection")));
			auto hout_unit   = query::entryPoint<compiler::helios::QueryModuleHOUT>(module);

			ASSERT_EQUAL(hout_unit.glob_data.size(), 3);

			std::vector<char> globals = { 'A', 'B', 'C' };
			query::utils::withContextDo([&](query::Context& ctx) {
				for (const auto& name: globals)
					ASSERT_TRUE(compiler::helios::isGlobalVar(
						ctx, find_global(hout_unit, base::StrID(name))->helios_symbol
					));

				auto var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
					&(*hout_unit.functions[0].content.body->statements[0])
				);

				ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));

				var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
					&(*hout_unit.functions[0].content.body->statements[1])
				);

				ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));

				var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
					&(*hout_unit.functions[1].content.body->statements[0])
				);

				ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));
			});
		}
	}

	/**
	 * This checks if all consts and vars in the module have proper types.
	 */
	void testTypeOfConstAndVar() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/type_deduction")));

		const auto int64_type = query::entryPoint<tsh::QueryIntegralType>({ 64, Signed });
		const auto bool_type  = query::entryPoint<tsh::QueryBoolType>({});
		const auto str_type   = query::entryPoint<tsh::QueryStringType>({});

		auto foo            = getChain("foo", root_scope).back();
		auto foo_body_scope = getFunctionBodyScope(foo);

		// @TODO: #925 fix how floats are deduced
		// @TODO: #925 fix how tuples are deduced

		// Vars
		ASSERT_EQUAL(int64_type, getTypeOf("EasyInt", foo_body_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("EasyBool", foo_body_scope));
		ASSERT_EQUAL(str_type, getTypeOf("EasyString", foo_body_scope));

		// Consts
		ASSERT_EQUAL(int64_type, getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("SimpleBool", root_scope));
		ASSERT_EQUAL(str_type, getTypeOf("SimpleString", root_scope));
	}

	void testDebugPrint() {
		auto [module, scope] = getModule(fs::File(path("test_modules/pretty_debug")));

		auto sym_c1         = getChain("c1", scope).back();
		auto sym_v1         = getChain("v1", scope).back();
		auto sym_n1         = getChain("n1", scope).back();
		auto sym_n2         = getChain("n1.n2", scope).back();
		auto sym_test_class = getChain("n1.n2.TestClass", scope).back();
		auto sym_n3         = getChain("n1.n3", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			std::cout << compiler::helios::prettyDebugPrint(sym_c1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_v1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n2, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_test_class, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n3, ctx) << '\n';
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")

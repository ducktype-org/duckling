// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_builder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/module_tree/script_module_tree.hpp>
#include <frontend/pst_parser/repl_utils/extractors.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class HeliosReplInstructionWrapperTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosReplInstructionWrapperTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testWrapperShapeForWhileInstruction);
		TESTER_ADD_TEST(testCounterAffectsMangledName);
		TESTER_ADD_TEST(testWrapperReturnTypeAndSymbolMetadata);
		TESTER_ADD_TEST(testWrapperAppendsVoidReturnForIfInstruction);
		TESTER_ADD_TEST(testSameKeyProducesStableWrapper);
		TESTER_ADD_TEST(testReplChainModulePathKeepsEnclosingModules);
	}

private:
	struct StmtKindCounter final: public helios::code::HoutStmtVisitorPanicky {
		usize while_count       = 0;
		usize if_count          = 0;
		usize void_return_count = 0;

		void visitWhileStmt(const helios::code::WhileStmt&) override { while_count++; }

		void visitIfStmt(const helios::code::IfStmt&) override { if_count++; }

		void visitVoidReturnStmt(const helios::code::VoidReturnStmt&) override {
			void_return_count++;
		}
	};

	static pst::AccessLocked<pst::Stmt> extractSingleInstruction(
		query::Context& ctx, std::string_view code
	) {
		return extractSingleInstruction(
			ctx, frontend::createModuleTreeFromContents(code, "test_pkg")
		);
	}

	static pst::AccessLocked<pst::Stmt> extractSingleInstruction(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = frontend::getFilePST(ctx, main_file);
		auto stmt_opt  = pst::extractSingleInstruction(ctx, pst->getRootElement());
		CORE_ASSERT(stmt_opt.has_value(), "Expected a single instruction in test input");
		return stmt_opt.value();
	}

	static CRef<query::QResult<helios::HOUTFunction>> instructionWrapper(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, u64 counter
	) {
		return ctx.query<helios::QueryCodeOfFun>(
			repl::queryReplInstructionWrapperSymbol(ctx, stmt, counter)
		);
	}

	void testWrapperShapeForWhileInstruction() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "while (1 == 1) {}");

			auto wrapper_result = instructionWrapper(ctx, stmt, 17);
			ASSERT_HAS_VALUE(*wrapper_result);
			auto& wrapper = wrapper_result->valueOrPanic();

			ASSERT_EQUAL(wrapper.declaration->parameters.size(), 0u);
			ASSERT_EQUAL(wrapper.body->statements.size(), 2u);

			StmtKindCounter counter;
			for (const auto& stmt_ref: wrapper.body->statements) stmt_ref->acceptVisitor(counter);

			ASSERT_EQUAL(counter.while_count, 1u);
			ASSERT_EQUAL(counter.void_return_count, 1u);

			auto sym_ref = helios::getSymRef(wrapper.declaration->original_symbol);
			ASSERT_MATCHES(sym_ref->other, helios::defgen::ReplInputWrapper);

			auto mangled
				= helios::mangler::getSimpleMangledName(ctx, wrapper.declaration->original_symbol);
			auto mangled_str = std::string(mangled.strView());
			assertTrue(
				mangled_str.find("__repl_input_wrapper_17") != std::string::npos,
				"Mangled name should include instruction-wrapper counter"
			);
		});
	}

	void testCounterAffectsMangledName() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "if (1 == 1) {}");

			auto wrapper_result_a = instructionWrapper(ctx, stmt, 21);
			auto wrapper_result_b = instructionWrapper(ctx, stmt, 22);
			ASSERT_HAS_VALUE(*wrapper_result_a);
			ASSERT_HAS_VALUE(*wrapper_result_b);
			auto& wrapper_a = wrapper_result_a->valueOrPanic();
			auto& wrapper_b = wrapper_result_b->valueOrPanic();

			auto mangled_a
				= helios::mangler::getSimpleMangledName(ctx, wrapper_a.declaration->original_symbol)
			          .strView();
			auto mangled_b
				= helios::mangler::getSimpleMangledName(ctx, wrapper_b.declaration->original_symbol)
			          .strView();

			assertTrue(
				mangled_a != mangled_b, "Different counters should produce different symbols"
			);
			assertTrue(
				std::string(mangled_a).find("__repl_input_wrapper_21") != std::string::npos,
				"First wrapper mangled name should include its counter"
			);
			assertTrue(
				std::string(mangled_b).find("__repl_input_wrapper_22") != std::string::npos,
				"Second wrapper mangled name should include its counter"
			);
		});
	}

	void testWrapperReturnTypeAndSymbolMetadata() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "while (1 == 1) {}");

			auto wrapper_result = instructionWrapper(ctx, stmt, 31);
			ASSERT_HAS_VALUE(*wrapper_result);
			auto& wrapper = wrapper_result->valueOrPanic();

			ASSERT_EQUAL(wrapper.declaration->parameters.size(), 0u);
			assertTrue(
				wrapper.declaration->return_type.getType() == tsh::getUnitType(),
				"Instruction wrapper return type should be unit"
			);

			auto sym = wrapper.declaration->original_symbol;
			ASSERT_EQUAL(helios::kind(sym), helios::SymbolKind::Function);

			auto sym_ref = helios::getSymRef(sym);

			auto repl_data = std::get_if<helios::defgen::ReplInputWrapper>(&sym_ref->other);
			assertTrue(repl_data != nullptr, "Expected ReplInputWrapper generated symbol");
			ASSERT_EQUAL(repl_data->counter.copyValueOr(0), 31u);
			ASSERT_MATCHES(repl_data->element, helios::defgen::ReplInputWrapper::Instruction);
		});
	}

	void testWrapperAppendsVoidReturnForIfInstruction() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "if (1 == 1) {}");

			auto wrapper_result = instructionWrapper(ctx, stmt, 41);
			ASSERT_HAS_VALUE(*wrapper_result);
			auto& wrapper = wrapper_result->valueOrPanic();

			ASSERT_EQUAL(wrapper.body->statements.size(), 2u);

			StmtKindCounter counter;
			for (const auto& stmt_ref: wrapper.body->statements) stmt_ref->acceptVisitor(counter);

			ASSERT_EQUAL(counter.if_count, 1u);
			ASSERT_EQUAL(counter.void_return_count, 1u);
		});
	}

	void testSameKeyProducesStableWrapper() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "if (1 == 1) {}");

			auto wrapper_result_a = instructionWrapper(ctx, stmt, 55);
			auto wrapper_result_b = instructionWrapper(ctx, stmt, 55);
			ASSERT_HAS_VALUE(*wrapper_result_a);
			ASSERT_HAS_VALUE(*wrapper_result_b);
			auto& wrapper_a = wrapper_result_a->valueOrPanic();
			auto& wrapper_b = wrapper_result_b->valueOrPanic();

			ASSERT_EQUAL(
				wrapper_a.declaration->original_symbol, wrapper_b.declaration->original_symbol
			);
			ASSERT_EQUAL(wrapper_a.body->statements.size(), wrapper_b.body->statements.size());

			auto mangled_a
				= helios::mangler::getSimpleMangledName(ctx, wrapper_a.declaration->original_symbol)
			          .strView();
			auto mangled_b
				= helios::mangler::getSimpleMangledName(ctx, wrapper_b.declaration->original_symbol)
			          .strView();

			ASSERT_EQUAL(std::string(mangled_a), std::string(mangled_b));
		});
	}

	void testReplChainModulePathKeepsEnclosingModules() {
		const auto package_id = base::StrID("repl_chain_mangling_package_id");

		auto root_builder = frontend::StandardModuleTreeBuilder::create();
		root_builder->setName(base::StrID("root"));
		root_builder->setPackageID(package_id);
		root_builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile("", ".dk"));
		auto root = root_builder->finalize();

		auto script_builder = frontend::ScriptModuleTreeBuilder::create();
		script_builder->setName(base::StrID("script"));
		script_builder->setPackageID(package_id);
		script_builder->setParent(root);
		auto script = script_builder->finalize();

		const auto make_node = [&](std::string_view                name,
		                           std::string_view                code,
		                           base::Ref<frontend::ModuleTree> parent) {
			auto builder = frontend::SyntheticReplChainModuleTreeBuilder::create();
			builder->setName(base::StrID(name));
			builder->setPackageID(package_id);
			builder->setSyntheticSourceFile(fs::FileManager::createRandomVirtualFile(code));
			builder->setParent(parent);
			return builder->finalize();
		};

		auto first  = make_node("first", "fun bar() = {}", script);
		auto second = make_node("second", "fun foo() = {}", first);

		const auto foo_scope = helios::test_utils::getModuleScope(second->getModuleID());
		const auto foo       = helios::test_utils::getChain("foo", foo_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto mangled = std::string(helios::mangler::getSimpleMangledName(ctx, foo).strView());

			// The modules enclosing the chain stay in the path, the earlier statements do not.
			assertTrue(
				mangled.find("4root6script6second") != std::string::npos,
				base::strConcat(
					"Mangled name should contain the enclosing modules and the statement module: ",
					mangled
				)
			);
			assertTrue(
				mangled.find("5first") == std::string::npos,
				base::strConcat(
					"Mangled name should not contain the previous statement of the chain: ", mangled
				)
			);
		});
	}

public:
	~HeliosReplInstructionWrapperTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/");

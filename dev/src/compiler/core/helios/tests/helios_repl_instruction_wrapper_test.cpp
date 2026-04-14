#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
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
		auto module_id = frontend::createModuleTreeFromContents(code, "test_pkg");
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = frontend::getFilePST(ctx, main_file);
		auto stmt_opt  = pst::extractSingleInstruction(ctx, pst->getRootElement());
		CORE_ASSERT(stmt_opt.has_value(), "Expected a single instruction in test input");
		return stmt_opt.value();
	}

	void testWrapperShapeForWhileInstruction() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "while (1 == 1) {}");

			auto wrapper = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 17,
			});

			ASSERT_EQUAL(wrapper.declaration->parameters.size(), 0u);
			ASSERT_EQUAL(wrapper.body->statements.size(), 2u);

			StmtKindCounter counter;
			for (const auto& stmt_ref: wrapper.body->statements) stmt_ref->acceptVisitor(counter);

			ASSERT_EQUAL(counter.while_count, 1u);
			ASSERT_EQUAL(counter.void_return_count, 1u);

			auto sym_ref  = helios::getSymRef(wrapper.declaration->original_symbol);
			auto gen_data = std::get_if<helios::defgen::GeneratedSymbolData>(&sym_ref->other);
			assertTrue(gen_data != nullptr, "Expected generated symbol data");
			assertTrue(
				std::holds_alternative<helios::defgen::GeneratedSymbolData::ReplInstructionWrapper>(
					gen_data->data
				),
				"Expected ReplInstructionWrapper generated symbol kind"
			);

			auto mangled
				= helios::mangler::getSimpleMangledName(ctx, wrapper.declaration->original_symbol);
			auto mangled_str = std::string(mangled.strView());
			assertTrue(
				mangled_str.find("__repl_instr_wrapper_17") != std::string::npos,
				"Mangled name should include instruction-wrapper counter"
			);
		});
	}

	void testCounterAffectsMangledName() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "if (1 == 1) {}");

			auto wrapper_a = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 21,
			});
			auto wrapper_b = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 22,
			});

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
				std::string(mangled_a).find("__repl_instr_wrapper_21") != std::string::npos,
				"First wrapper mangled name should include its counter"
			);
			assertTrue(
				std::string(mangled_b).find("__repl_instr_wrapper_22") != std::string::npos,
				"Second wrapper mangled name should include its counter"
			);
		});
	}

	void testWrapperReturnTypeAndSymbolMetadata() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "while (1 == 1) {}");

			auto wrapper = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 31,
			});

			ASSERT_EQUAL(wrapper.declaration->parameters.size(), 0u);
			assertTrue(
				wrapper.declaration->return_type.getType() == tsh::getUnitType(),
				"Instruction wrapper return type should be unit"
			);

			auto sym = wrapper.declaration->original_symbol;
			ASSERT_EQUAL(helios::kind(sym), helios::SymbolKind::Function);

			auto sym_ref  = helios::getSymRef(sym);
			auto gen_data = std::get_if<helios::defgen::GeneratedSymbolData>(&sym_ref->other);
			assertTrue(gen_data != nullptr, "Expected generated symbol data");

			auto repl_data
				= std::get_if<helios::defgen::GeneratedSymbolData::ReplInstructionWrapper>(
					&gen_data->data
				);
			assertTrue(repl_data != nullptr, "Expected ReplInstructionWrapper generated symbol");
			ASSERT_EQUAL(repl_data->counter, 31u);
		});
	}

	void testWrapperAppendsVoidReturnForIfInstruction() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto stmt = extractSingleInstruction(ctx, "if (1 == 1) {}");

			auto wrapper = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 41,
			});

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

			auto wrapper_a = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 55,
			});
			auto wrapper_b = ctx.query<repl::QueryReplInstructionWrapper>({
				.stmt    = stmt,
				.counter = 55,
			});

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

public:
	~HeliosReplInstructionWrapperTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/");

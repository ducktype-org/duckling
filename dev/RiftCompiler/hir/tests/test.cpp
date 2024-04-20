#include <tester/tester.hpp>
#include <hir/hir.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>

class HirTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HirTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("HIR test") {
		lexer::init();
		pst::init();
		ts::init();
		exec::init();

		TESTER_ADD_TEST(testI32Consts);
	}

private:
	hir::SourceUnit prepare(const fs::FilePath& file) {
		auto td     = lexer::tokenizeFile(file);
		auto parsed = pst::parse(std::move(td));
		assert(parsed.getLogger().good(), "there are unexpected errors in rift source-code");
		return { std::move(parsed), file };
	}

	symtable::SymbolRef getSymbolFromLookupIn(symtable::SymbolRef symbol, base::StrId name) {
		auto lookup_result = symbol->requestLookupIn(name);
		assert(lookup_result.isSingle(), "Lookup result not a single symbol.");
		auto dealiased_single = symtable::deAliasSymbolChain(lookup_result.getAsSingle());
		return dealiased_single.back();
	}

	void testI32Consts() {
		hir::HIR hir;
		{
			auto source = prepare(fs::FilePath(path("constants.rift")));
			hir.addUnit(std::move(source));
		}
		hir.doMagicStuff();

		auto root_scope = hir.getState().symTable().getRootScope();
		assert(
			root_scope->getSymbols().size() == 1, "Root scope does not has single sub root scope"
		);

		auto top_level_symbol = root_scope->getSymbols()[0];

		auto get_symbol_from_top_level
			= [&](auto name) { return getSymbolFromLookupIn(top_level_symbol, base::StrId(name)); };

		auto C = get_symbol_from_top_level("C");
		assert(C->requestValue().getData<i32>().back() == 1, "Bad value of C");

		auto A = get_symbol_from_top_level("A");
		assert(A->requestValue().getData<i32>().back() == 1, "Bad value of A");

		auto B = get_symbol_from_top_level("B");
		assert(B->requestValue().getData<i32>().back() == -3, "Bad value of B");

		auto D = get_symbol_from_top_level("D");
		assert(D->requestValue().getData<i32>().back() == -1, "Bad value of D");

		auto H2 = get_symbol_from_top_level("H2");
		assert(H2->requestValue().getData<i32>().back() == 3, "Bad value of H2");

		auto T0 = get_symbol_from_top_level("T0");
		auto T1 = get_symbol_from_top_level("T1");
		auto T2 = get_symbol_from_top_level("T2");
		assert(T0->requestValue().getData<i32>().back() == 1, "Bad value of T0");
		assert(T1->requestValue().getData<i32>().back() == 2, "Bad value of T1");
		assert(T2->requestValue().getData<i32>().back() == 3, "Bad value of T2");
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/hir/tests/");

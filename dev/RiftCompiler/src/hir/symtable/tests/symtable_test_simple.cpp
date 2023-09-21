// @TODO: change to reflect moving symtable to hir
// deleted for now

#include <filesystem/file.hpp>
#include <pst_parser/parser.hpp>
#include <hir/symtable/symtable.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

class SimpleSymtableTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleSymtableTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Symtable Test") {
		lexer::init();
		pst::init();

		// TESTER_ADD_TEST(simple);
		// TESTER_ADD_TEST(connections);
		// TESTER_ADD_TEST(SimpleSymtableTest, two_tables);
		// TESTER_ADD_TEST(simpleSymtableFillingTest);
		// TESTER_ADD_TEST(lookupTestOnRealFile);
	}

private:
	pst::PST prepare(std::string filename) {
		fs::FilePath file(filename);
		auto td = lexer::tokenizeFile(file);
		return pst::parse(std::move(td));
	}
/*
	void simple() {
		auto bad_id = symtable::ScopeId::bad();
		symtable::SymbolTable symbolTable;
		auto id = symbolTable.newScope(bad_id);
		auto id_2 = symbolTable.newScope(id);

		assert(id != id_2, "ids should differ");

		// @TODO: scope can be copied for now (that is not what we want)
		auto& scope_1 = symbolTable.get(id);
		auto& scope_2 = symbolTable.get(id_2);

		assert(scope_1.getParent() == bad_id, "parent incorrect 0");
		assert(scope_2.getParent() == id, "parent incorrect 1");

		auto s_id_data = new symtable::Symbol(id, base::StrId("mysymbol"), false, false);
		auto s_id_1_data = new symtable::Symbol(id, base::StrId("mysymbol_1"), false, true);
		auto s_id_2_data = new symtable::Symbol(id_2, base::StrId("mysymbol_2"), false, false);

		auto s_id = symbolTable.newSymbol(base::unique_ptr(s_id_data));
		auto s_id_1 = symbolTable.newSymbol(base::unique_ptr(s_id_1_data));
		auto s_id_2 = symbolTable.newSymbol(base::unique_ptr(s_id_2_data));

		auto symbol = symbolTable.get(s_id);
		auto symbol_1 = symbolTable.get(s_id_1);
		auto symbol_2 = symbolTable.get(s_id_2);

		assert(symbol->getScope() == id, "bad scope 0");
		assert(symbol_1->getScope() == id, "bad scope 1");
		assert(symbol_2->getScope() == id_2, "bad scope 2");

		assert(symbol->getName().str() == "mysymbol", "incorrect name 0");
		assert(symbol_1->getName().str() == "mysymbol_1", "incorrect name 1");
		assert(symbol_2->getName().str() == "mysymbol_2", "incorrect name 2");

		assert(symbol->getIsStatic() == false, "incorrect isStatic 0");
		assert(symbol_1->getIsStatic() == true, "incorrect isStatic 1");
		assert(symbol_2->getIsStatic() == false, "incorrect isStatic 2");
		
		auto& symbols_1 = scope_1.getSymbols();
		auto& symbols_2 = scope_2.getSymbols();

		assert(symbols_1.size() == 2, "bad amount of symbols in scope 1");
		assert(symbols_2.size() == 1, "bad amount of symbols in scope 2");

		assert(symbols_1[0] == s_id && symbols_1[1] == s_id_1, "wrong list of symbols in scope 1");
		assert(symbols_2[0] == s_id_2, "wrong list of symbols in scope 2");
	}

	void connections() {
		auto bad_id = symtable::ScopeId::bad();
		symtable::SymbolTable symbolTable;


		std::vector<symtable::ScopeId> scopes;
		scopes.push_back(bad_id);

		for (usize i = 0; i < 10; i++) {
			auto id = symbolTable.newScope(scopes.back());
			scopes.push_back(id);
		}

		for (usize i = 1; i < 10; i++) {
			for (usize j = 0; j < 10; j++) {

				if (i < j) {
					symbolTable.addConnection(
						symtable::ConnectionType::ImportPrivate, scopes[i], scopes[j]);
				} else {
					symbolTable.addConnection(
						symtable::ConnectionType::ImportPublic, scopes[i], scopes[j]);
				}
			}
		}

		// @TODO: for now just checking size, some more complicated test needed
		for (usize i = 1; i < 10; i++) {
			auto& imports_private =
				symbolTable.get(scopes[i]).getConnections(symtable::ConnectionType::ImportPrivate);
			assert(imports_private.size() == 9 - i, "wrong private imports count");
			auto& imports_public =
				symbolTable.get(scopes[i]).getConnections(symtable::ConnectionType::ImportPublic);
			assert(imports_public.size() == i + 1, "wrong public imports count");
		}
	}

	// @TODO: this test is not finished, it should test creating two symtables at once
	void two_tables() {
		symtable::SymbolTable symbolTable_1;
		symtable::SymbolTable symbolTable_2;

		auto bad_id = symtable::ScopeId::bad();
		auto id_1 = symbolTable_1.newScope(bad_id);
		auto id_2 = symbolTable_2.newScope(bad_id);

		// auto id_3 = symbolTable_1

		assert(id_1 != id_2, "ids should differ");
	}

	template<typename... T>
	static std::vector<base::StrId> strIdVectorMaker(T&&... args) {
		std::vector<base::StrId> out;
		(out.emplace_back(args), ...);
		return out;
	}

	void simpleSymtableFillingTest() {
		pst::PST pst = prepare(path("snippets/lot_of_symbols.rift"));
		assert(pst.getErrorState().good(), "there are unexpected errors in rift source-code");

		symtable::SymbolTable sym_table;
		pst.fillSymTable(sym_table);


		assert(sym_table.symbolCount() == 16,
		       base::strConcat("wrong number of symbols in fun.rift: ",
		                        sym_table.symbolCount(),
		                        " instead of 16.",
		                        std::make_tuple("abcd")));
	}

	@deprecated
	void lookupTestOnRealFile() {

		pst::PST pst = prepare(path("snippets/symbol_in_namespaces.rift"));
		assert(pst.getErrorState().good(), "there are unexpected errors in rift source-code");
		symtable::SymbolTable sym_table;
		pst.fillSymTable(sym_table);

		// Top-level scopes:

		symtable::ScopeId root_scope = sym_table.getRootScope();
		assert(root_scope.isGood(), "root scope is not good");

		symtable::ScopeId pst_root_scope = pst.getRootScope();
		assert(pst_root_scope.isGood(), "pst root scope is not good");

		// Standard lookups:

		auto A = sym_table.lookup(pst_root_scope, base::StrId("A"));
		auto B = sym_table.lookup(pst_root_scope, base::StrId("B"));
		assert(A.isGood(), "A symbol was not found");
		assert(A.isGood(), "B symbol was not found");

		// this should fail, and might need a change when other error handling is introduced:
		auto badC = sym_table.lookup(pst_root_scope, base::StrId("C"));
		assert(badC.isBad(), "C symbol was found is global scope");

		// Linked lookups:

		auto D = sym_table.lookupLinked(A, base::StrId("D"));
		assert(D.isGood(), "D symbol was not found in A");

		auto foo = sym_table.lookupLinked(A, base::StrId("foo"));
		assert(foo.isGood(), "foo symbol was not found in A");

		auto foo2 = sym_table.lookupLinked(D, base::StrId("foo2"));
		assert(foo2.isGood(), "D symbol was not found in D");

		auto C = sym_table.lookupLinked(B, base::StrId("C"));
		assert(C.isGood(), "C symbol was not found in B");

		auto E = sym_table.lookupLinked(C, base::StrId("E"));
		assert(E.isGood(), "E symbol was not found in C");

		auto foo3 = sym_table.lookupLinked(E, base::StrId("foo3"));
		assert(foo3.isGood(), "foo3 symbol was not found in E");

		auto random_bad = sym_table.lookupLinked(E, base::StrId("random_bad"));
		assert(random_bad.isBad(), "random_bad symbol was found in E");

		// QNL lookup from top-level:
		{
			auto qnl_A = sym_table.QNL(pst_root_scope, strIdVectorMaker("A"));
			assert(A.isGood(), "A symbol was not found by QNL");
			assert(qnl_A == A, "A symbol is different from the one found by QNL");

			auto qnl_foo2 = sym_table.QNL(pst_root_scope, strIdVectorMaker("A", "D", "foo2"));
			assert(qnl_foo2.isGood(), "foo2 symbol was not found by QNL");
			assert(qnl_foo2 == foo2, "A symbol is different from the one found by QNL");

			auto qnl_foo3 = sym_table.QNL(pst_root_scope, strIdVectorMaker("B", "C", "E", "foo3"));
			assert(qnl_foo3.isGood(), "foo3 symbol was not found by QNL");
			assert(qnl_foo3 == foo3, "A symbol is different from the one found by QNL");
		}

		// QNL lookup from inner scopes:
		{
			auto linked_C_scope = sym_table.get(C)->getLinkedLookupScope();

			auto qnl_A = sym_table.QNL(linked_C_scope, strIdVectorMaker("A"));
			assert(A.isGood(), "A symbol was not found by QNL from C");
			assert(qnl_A == A, "A symbol is different from the one found by QNL from C");

			auto qnl_foo2 = sym_table.QNL(linked_C_scope, strIdVectorMaker("A", "D", "foo2"));
			assert(qnl_foo2.isGood(), "foo2 symbol was not found by QNL from C");
			assert(qnl_foo2 == foo2, "A symbol is different from the one found by QNL from C");

			auto qnl_foo3 = sym_table.QNL(linked_C_scope, strIdVectorMaker("E", "foo3"));
			assert(qnl_foo3.isGood(), "foo3 symbol was not found by QNL from C");
			assert(qnl_foo3 == foo3, "A symbol is different from the one found by QNL from C");
		}
	}
*/
public:
	~SimpleSymtableTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/symtable/tests/");

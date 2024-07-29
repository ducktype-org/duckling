#include <helios/scope_symbol_id.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>
#include <helios/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>
#include <typesystem/internal/queries.hpp>

class ConfigTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConfigTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("ConfigTests") {
		lexer::init();
		pst::init();

	}

private:
	
};

TESTER_COMMON_MAIN("/RiftCompiler/config/tests/");

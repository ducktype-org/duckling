#include <c_import/identifiers.hpp>

#include <tester/tester.hpp>

#include <string>

using namespace c_import;

class CImportIdentifiersTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportIdentifiersTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(keywordsTest);
		TESTER_ADD_TEST(tagPrefixingTest);
		TESTER_ADD_TEST(registryTest);
	}

private:
	void keywordsTest() {
		ASSERT_TRUE(isDucklingKeyword("class"));
		ASSERT_TRUE(isDucklingKeyword("fundecl"));
		ASSERT_TRUE(isDucklingKeyword("cptr"));
		ASSERT_TRUE(isDucklingKeyword("in"));
		ASSERT_TRUE(isDucklingKeyword("new"));
		ASSERT_TRUE(isDucklingKeyword("type"));
		ASSERT_TRUE(isDucklingKeyword("Array"));
		ASSERT_TRUE(isDucklingKeyword("i32"));

		ASSERT_EQUAL(false, isDucklingKeyword("mylib_add"));
		ASSERT_EQUAL(false, isDucklingKeyword("printf"));
		// Capitalisation matters: only `Array`/`Dict`/`Set` are keywords.
		ASSERT_EQUAL(false, isDucklingKeyword("CLASS"));
		ASSERT_EQUAL(false, isDucklingKeyword(""));
	}

	void tagPrefixingTest() {
		ASSERT_EQUAL(std::string("struct_stat"), taggedRecordName("stat"));
		ASSERT_EQUAL(std::string("union_sigval"), taggedUnionName("sigval"));
		ASSERT_EQUAL(std::string("enum_color"), taggedEnumName("color"));

		// `struct stat` and `int stat()` coexist in C; prefixing keeps them apart here.
		NameRegistry registry;
		ASSERT_TRUE(registry.claim(taggedRecordName("stat")).accepted);
		ASSERT_TRUE(registry.claim("stat").accepted);
	}

	void registryTest() {
		NameRegistry registry;

		ASSERT_TRUE(registry.claim("mylib_add").accepted);
		ASSERT_TRUE(registry.isClaimed("mylib_add"));

		// First one wins, the second is reported rather than shadowing it.
		auto duplicate = registry.claim("mylib_add");
		ASSERT_EQUAL(false, duplicate.accepted);
		ASSERT_EQUAL(false, duplicate.reason.empty());

		// Renaming a keyword would break the link, so it is rejected outright.
		auto keyword = registry.claim("match");
		ASSERT_EQUAL(false, keyword.accepted);
		ASSERT_EQUAL(false, registry.isClaimed("match"));

		auto unnamed = registry.claim("");
		ASSERT_EQUAL(false, unnamed.accepted);
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/c_import_identifiers_test.cpp")

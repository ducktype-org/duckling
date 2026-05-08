#include <json/extract.hpp>

#include <tester/tester.hpp>

#include <array>
#include <string_view>

class JsonExtractTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JsonExtractTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(extractStringSuccess);
		TESTER_ADD_TEST(extractStringMissingKey);
		TESTER_ADD_TEST(extractStringWrongType);
		TESTER_ADD_TEST(extractStringNotAnObject);
		TESTER_ADD_TEST(extractBoolSuccess);
		TESTER_ADD_TEST(extractBoolWrongType);
		TESTER_ADD_TEST(extractArraySuccess);
		TESTER_ADD_TEST(extractArrayWrongType);
		TESTER_ADD_TEST(extractObjectSuccess);
		TESTER_ADD_TEST(extractStringValueSuccess);
		TESTER_ADD_TEST(extractStringValueWrongType);
		TESTER_ADD_TEST(findUnknownFieldsAllKnown);
		TESTER_ADD_TEST(findUnknownFieldsMixed);
		TESTER_ADD_TEST(findUnknownFieldsNotAnObject);
		TESTER_ADD_TEST(isObjectPredicate);
	}

private:
	void extractStringSuccess() {
		auto j      = nlohmann::json::parse(R"({"name": "duck"})");
		auto result = js::extractString(j, "name");
		ASSERT_TRUE(result.has_value());
		ASSERT_EQUAL(result->str(), std::string("duck"));
	}

	void extractStringMissingKey() {
		auto j      = nlohmann::json::parse(R"({"other": "x"})");
		auto result = js::extractString(j, "name");
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::MissingKey);
	}

	void extractStringWrongType() {
		auto j      = nlohmann::json::parse(R"({"name": 42})");
		auto result = js::extractString(j, "name");
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::WrongType);
	}

	void extractStringNotAnObject() {
		auto j      = nlohmann::json::parse(R"([1, 2, 3])");
		auto result = js::extractString(j, "name");
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::NotAnObject);
	}

	void extractBoolSuccess() {
		auto j      = nlohmann::json::parse(R"({"flag": true})");
		auto result = js::extractBool(j, "flag");
		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(*result);
	}

	void extractBoolWrongType() {
		auto j      = nlohmann::json::parse(R"({"flag": "yes"})");
		auto result = js::extractBool(j, "flag");
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::WrongType);
	}

	void extractArraySuccess() {
		auto j      = nlohmann::json::parse(R"({"xs": [1, 2, 3]})");
		auto result = js::extractArray(j, "xs");
		ASSERT_TRUE(result.has_value());
		ASSERT_EQUAL(result->size(), 3u);
	}

	void extractArrayWrongType() {
		auto j      = nlohmann::json::parse(R"({"xs": "not an array"})");
		auto result = js::extractArray(j, "xs");
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::WrongType);
	}

	void extractObjectSuccess() {
		auto j      = nlohmann::json::parse(R"({"inner": {"k": "v"}})");
		auto result = js::extractObject(j, "inner");
		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(result->is_object());
	}

	void extractStringValueSuccess() {
		auto j      = nlohmann::json::parse(R"("standalone")");
		auto result = js::extractStringValue(j);
		ASSERT_TRUE(result.has_value());
		ASSERT_EQUAL(result->str(), std::string("standalone"));
	}

	void extractStringValueWrongType() {
		auto j      = nlohmann::json::parse(R"(42)");
		auto result = js::extractStringValue(j);
		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(result.error() == js::JsonExtractError::WrongType);
	}

	void findUnknownFieldsAllKnown() {
		auto j = nlohmann::json::parse(R"({"a": 1, "b": 2})");
		std::array<std::string_view, 1> required{ "a" };
		std::array<std::string_view, 1> optional{ "b" };
		auto unknown = js::findUnknownFields(j, required, optional);
		ASSERT_TRUE(unknown.empty());
	}

	void findUnknownFieldsMixed() {
		auto j = nlohmann::json::parse(R"({"a": 1, "extra": 2, "b": 3, "weird": 4})");
		std::array<std::string_view, 1> required{ "a" };
		std::array<std::string_view, 1> optional{ "b" };
		auto unknown = js::findUnknownFields(j, required, optional);
		ASSERT_EQUAL(unknown.size(), 2u);
	}

	void findUnknownFieldsNotAnObject() {
		auto j = nlohmann::json::parse(R"([1, 2, 3])");
		std::array<std::string_view, 0> required{};
		std::array<std::string_view, 0> optional{};
		auto unknown = js::findUnknownFields(j, required, optional);
		ASSERT_TRUE(unknown.empty());
	}

	void isObjectPredicate() {
		auto obj = nlohmann::json::parse(R"({})");
		auto arr = nlohmann::json::parse(R"([])");
		ASSERT_TRUE(js::isObject(obj));
		ASSERT_TRUE(!js::isObject(arr));
	}

public:
	~JsonExtractTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/json/tests/");

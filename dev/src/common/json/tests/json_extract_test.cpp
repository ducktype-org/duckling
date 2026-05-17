#include <tester/tester.hpp>

#include <json/diagnostics.hpp>
#include <json/extract.hpp>

#include <array>
#include <string_view>

namespace {
	struct TestReporter {
		int errors   = 0;
		int warnings = 0;

		js::DiagnosticLogger callback() {
			return [this](std::string_view, std::string_view, bool is_error) {
				if (is_error)
					++errors;
				else
					++warnings;
			};
		}
	};
}

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
		TESTER_ADD_TEST(diagnosticsReporters);
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
		auto                            j = nlohmann::json::parse(R"({"a": 1, "b": 2})");
		std::array<std::string_view, 1> required{ "a" };
		std::array<std::string_view, 1> optional{ "b" };
		auto                            unknown = js::findUnknownFields(j, required, optional);
		ASSERT_TRUE(unknown.empty());
	}

	void findUnknownFieldsMixed() {
		auto j = nlohmann::json::parse(R"({"a": 1, "extra": 2, "b": 3, "weird": 4})");
		std::array<std::string_view, 1> required{ "a" };
		std::array<std::string_view, 1> optional{ "b" };
		auto                            unknown = js::findUnknownFields(j, required, optional);
		ASSERT_EQUAL(unknown.size(), 2u);
	}

	void findUnknownFieldsNotAnObject() {
		auto                            j = nlohmann::json::parse(R"([1, 2, 3])");
		std::array<std::string_view, 0> required{};
		std::array<std::string_view, 0> optional{};
		auto                            unknown = js::findUnknownFields(j, required, optional);
		ASSERT_TRUE(unknown.empty());
	}

	void isObjectPredicate() {
		auto obj = nlohmann::json::parse(R"({})");
		auto arr = nlohmann::json::parse(R"([])");
		ASSERT_TRUE(js::isObject(obj));
		ASSERT_TRUE(!js::isObject(arr));
	}

	void diagnosticsReporters() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "name": 123,
            "flag": "yes",
            "array": "no",
            "obj": "no",
            "extra": true
        })");

		ASSERT_TRUE(!js::checkIsObject(nlohmann::json::parse(R"([1])"), "root", reporter.callback())
		);
		ASSERT_TRUE(!js::getString(json, "name", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getStringIfPresent(json, "name", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getBool(json, "flag", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getBoolIfPresent(json, "flag", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getArray(json, "array", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getObject(json, "obj", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getObjectIfPresent(json, "obj", "bad", reporter.callback()).has_value());
		ASSERT_TRUE(
			!js::getStringValue(nlohmann::json::parse("123"), "value", "bad", reporter.callback())
				 .has_value()
		);

		auto warn_json = nlohmann::json::parse(R"({"missing": 1, "items": ["ok", 2]})");
		ASSERT_TRUE(!js::getStringWarning(warn_json, "nope", "warn", reporter.callback()).has_value()
		);
		ASSERT_TRUE(!js::getBoolWarning(warn_json, "nope", "warn", reporter.callback()).has_value());
		ASSERT_TRUE(!js::getArrayWarning(warn_json, "nope", "warn", reporter.callback()).has_value()
		);
		ASSERT_TRUE(!js::getObjectWarning(warn_json, "nope", "warn", reporter.callback()).has_value()
		);

		auto elem_warn
			= js::getStringFromArrayWarning(warn_json["items"][1], "items", reporter.callback());
		ASSERT_TRUE(!elem_warn.has_value());
		auto elem_err = js::getStringFromArray(warn_json["items"][1], "items", reporter.callback());
		ASSERT_TRUE(!elem_err.has_value());

		js::checkForUnknownFields(
			warn_json, { "missing" }, { "items" }, "root", reporter.callback()
		);

		ASSERT_TRUE(reporter.errors > 0);
		ASSERT_TRUE(reporter.warnings > 0);
	}

public:
	~JsonExtractTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/json/tests/");

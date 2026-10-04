#include "diagnostics.hpp"

#include "extract.hpp"

#include <base/str/str_utils.hpp>

#include <nlohmann/json.hpp>

#include <expected>
#include <string>
#include <utility>

namespace js {

	namespace {
		void reportFieldError(
			const DiagnosticLogger& report,
			std::string_view        field_type,
			std::string_view        key,
			std::string_view        message,
			bool                    is_error
		) {
			report(base::strConcat("No ", field_type, " value with key: ", key), message, is_error);
		}

		void reportValueError(
			const DiagnosticLogger& report,
			std::string_view        field_type,
			std::string_view        value_name,
			std::string_view        message
		) {
			report(base::strConcat("No ", field_type, " value for: ", value_name), message, true);
		}

		template<class T>
		base::Optional<T> toOpt(std::expected<T, JsonExtractError> result) {
			if (result.has_value()) return std::move(*result);
			return {};
		}
	}  // namespace

	bool checkIsObject(
		const nlohmann::json& json, std::string_view context_name, const DiagnosticLogger& report
	) {
		if (isObject(json)) return true;
		report(
			base::strConcat(context_name, " must be a JSON object"),
			base::strConcat(
				"The value for ", context_name, " is not a JSON object and cannot be parsed."
			),
			true
		);
		return false;
	}

	base::Optional<base::StrID> getStringWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	) {
		auto result = extractString(json, key);
		if (!result.has_value()) reportFieldError(report, "string", key, warning_message, false);
		return toOpt(std::move(result));
	}

	base::Optional<base::StrID> getString(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractString(json, key);
		if (!result.has_value()) reportFieldError(report, "string", key, error_message, true);
		return toOpt(std::move(result));
	}

	base::Optional<base::StrID> getStringIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractString(json, key);
		if (result.has_value()) return *result;
		if (result.error() == JsonExtractError::WrongType)
			reportFieldError(report, "string", key, error_message, true);
		return {};
	}

	base::Optional<base::StrID> getStringValue(
		const nlohmann::json&   json,
		std::string_view        context_name,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractStringValue(json);
		if (result.has_value()) return *result;
		reportValueError(report, "string", context_name, error_message);
		return {};
	}

	base::Optional<bool> getBoolWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	) {
		auto result = extractBool(json, key);
		if (!result.has_value()) reportFieldError(report, "bool", key, warning_message, false);
		return toOpt(std::move(result));
	}

	base::Optional<bool> getBool(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractBool(json, key);
		if (!result.has_value()) reportFieldError(report, "bool", key, error_message, true);
		return toOpt(std::move(result));
	}

	base::Optional<bool> getBoolIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractBool(json, key);
		if (result.has_value()) return *result;
		if (result.error() == JsonExtractError::WrongType)
			reportFieldError(report, "bool", key, error_message, true);
		return {};
	}

	base::Optional<std::vector<nlohmann::json>> getArrayWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	) {
		auto result = extractArray(json, key);
		if (!result.has_value()) reportFieldError(report, "array", key, warning_message, false);
		return toOpt(std::move(result));
	}

	base::Optional<std::vector<nlohmann::json>> getArray(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractArray(json, key);
		if (!result.has_value()) reportFieldError(report, "array", key, error_message, true);
		return toOpt(std::move(result));
	}

	base::Optional<nlohmann::json> getObjectWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	) {
		auto result = extractObject(json, key);
		if (!result.has_value()) reportFieldError(report, "object", key, warning_message, false);
		return toOpt(std::move(result));
	}

	base::Optional<nlohmann::json> getObject(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractObject(json, key);
		if (!result.has_value()) reportFieldError(report, "object", key, error_message, true);
		return toOpt(std::move(result));
	}

	base::Optional<nlohmann::json> getObjectIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	) {
		auto result = extractObject(json, key);
		if (result.has_value()) return std::move(*result);
		if (result.error() == JsonExtractError::WrongType)
			reportFieldError(report, "object", key, error_message, true);
		return {};
	}

	base::Optional<base::StrID> getStringFromArrayWarning(
		const nlohmann::json& elem, std::string_view parent_name, const DiagnosticLogger& report
	) {
		auto result = extractStringValue(elem);
		if (result.has_value()) return *result;
		report(
			base::strConcat("Non-string value in array of ", parent_name),
			base::strConcat(
				"In ",
				parent_name,
				": a non-string value was found in the array and will be ignored."
			),
			false
		);
		return {};
	}

	base::Optional<base::StrID> getStringFromArray(
		const nlohmann::json& elem, std::string_view parent_name, const DiagnosticLogger& report
	) {
		auto result = extractStringValue(elem);
		if (result.has_value()) return *result;
		report(
			base::strConcat("Non-string value in array of ", parent_name),
			base::strConcat("In ", parent_name, ": a non-string value was found in the array."),
			true
		);
		return {};
	}

	void checkForUnknownFields(
		const nlohmann::json&                   json,
		std::initializer_list<std::string_view> required_keys,
		std::initializer_list<std::string_view> optional_keys,
		std::string_view                        context,
		const DiagnosticLogger&                 report
	) {
		std::vector<std::string_view> required(required_keys);
		std::vector<std::string_view> optional(optional_keys);
		auto                          unknown = findUnknownFields(json, required, optional);
		for (const auto& key: unknown) {
			report(
				base::strConcat("Unknown field: ", key),
				base::strConcat(
					"In ", context, ": field \"", key, "\" is not recognized and will be ignored."
				),
				false
			);
		}
	}

}  // namespace js

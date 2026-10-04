/**
 * @file diagnostics.hpp
 * @brief JSON extraction helpers that report diagnostics through a callback.
 */
#pragma once

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace js {

	/**
	 * @brief Callback for reporting JSON extraction diagnostics.
	 */
	using DiagnosticLogger
		= std::function<void(std::string_view header, std::string_view description, bool is_error)>;

	/** @brief Verify @p json is an object; otherwise report an error. */
	bool checkIsObject(
		const nlohmann::json& json, std::string_view context_name, const DiagnosticLogger& report
	);

	/** @brief Extract a string field; report a warning on failure. */
	base::Optional<base::StrID> getStringWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract a string field; report an error on failure. */
	base::Optional<base::StrID> getString(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract a string if the key is present; report only on present-but-wrong-type. */
	base::Optional<base::StrID> getStringIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Interpret a JSON value as a string; report an error on failure. */
	base::Optional<base::StrID> getStringValue(
		const nlohmann::json&   json,
		std::string_view        context_name,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract a boolean field; report a warning on failure. */
	base::Optional<bool> getBoolWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract a boolean field; report an error on failure. */
	base::Optional<bool> getBool(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract a boolean if the key is present; report only on present-but-wrong-type. */
	base::Optional<bool> getBoolIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract an array field; report a warning on failure. */
	base::Optional<std::vector<nlohmann::json>> getArrayWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract an array field; report an error on failure. */
	base::Optional<std::vector<nlohmann::json>> getArray(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract an object field; report a warning on failure. */
	base::Optional<nlohmann::json> getObjectWarning(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        warning_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract an object field; report an error on failure. */
	base::Optional<nlohmann::json> getObject(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Extract an object if the key is present; report only on present-but-wrong-type. */
	base::Optional<nlohmann::json> getObjectIfPresent(
		const nlohmann::json&   json,
		std::string_view        key,
		std::string_view        error_message,
		const DiagnosticLogger& report
	);

	/** @brief Interpret a JSON array element as a string; report a warning on failure. */
	base::Optional<base::StrID> getStringFromArrayWarning(
		const nlohmann::json& elem, std::string_view parent_name, const DiagnosticLogger& report
	);

	/** @brief Interpret a JSON array element as a string; report an error on failure. */
	base::Optional<base::StrID> getStringFromArray(
		const nlohmann::json& elem, std::string_view parent_name, const DiagnosticLogger& report
	);

	/** @brief Report a warning for every key in @p json not in @p required or @p optional. */
	void checkForUnknownFields(
		const nlohmann::json&                   json,
		std::initializer_list<std::string_view> required_keys,
		std::initializer_list<std::string_view> optional_keys,
		std::string_view                        context,
		const DiagnosticLogger&                 report
	);

}  // namespace js

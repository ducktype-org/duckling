#include "utils.hpp"

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <json/json.hpp>

#include <unordered_set>

namespace compiler::driver::json {

	namespace {
		void logFieldError(
			std::string_view field_type, const std::string& key, const std::string& error_message
		) {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
				base::strConcat("No ", field_type, " value with key: ", key), error_message
			));
		}

		void logWarning(const std::string& header, const std::string& description = "") {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(
				makeBox<dia_int::PlaceholderWarning>(header, description)
			);
		}

		void logFieldWarning(
			std::string_view field_type, const std::string& key, const std::string& warning_message
		) {
			logWarning(
				base::strConcat("No ", field_type, " value with key: ", key), warning_message
			);
		}

		void logValueError(
			std::string_view   field_type,
			const std::string& value_name,
			const std::string& error_message
		) {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
				base::strConcat("No ", field_type, " value for: ", value_name), error_message
			));
		}

	}  // namespace

	base::Optional<base::StrID> getStringNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_string()) return {};
		return base::StrID(json[key].get<std::string>());
	}

	base::Optional<base::StrID> getStringWarning(
		const nlohmann::json& json, const Key& key, const std::string& warning_message
	) {
		auto result = getStringNoError(json, key);
		if (!result.has_value()) logFieldWarning("string", key, warning_message);
		return result;
	}

	base::Optional<base::StrID> getString(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		auto result = getStringNoError(json, key);
		if (!result.has_value()) logFieldError("string", key, error_message);
		return result;
	}

	base::Optional<base::StrID> getStringIfPresent(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		if (!json.contains(key)) return {};
		if (!json[key].is_string()) {
			logFieldError("string", key, error_message);
			return {};
		}
		return base::StrID(json[key].get<std::string>());
	}

	base::Optional<base::StrID> getStringValue(
		const nlohmann::json& json, const std::string& context_name, const std::string& error_message
	) {
		if (!json.is_string()) {
			logValueError("string", context_name, error_message);
			return {};
		}
		return base::StrID(json.get<std::string>());
	}

	base::Optional<bool> getBoolNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_boolean()) return {};
		return json[key].get<bool>();
	}

	base::Optional<bool> getBoolWarning(
		const nlohmann::json& json, const Key& key, const std::string& warning_message
	) {
		auto result = getBoolNoError(json, key);
		if (!result.has_value()) logFieldWarning("bool", key, warning_message);
		return result;
	}

	base::Optional<bool> getBool(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		auto result = getBoolNoError(json, key);
		if (!result.has_value()) logFieldError("bool", key, error_message);
		return result;
	}

	base::Optional<bool> getBoolIfPresent(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		if (!json.contains(key)) return {};
		if (!json[key].is_boolean()) {
			logFieldError("bool", key, error_message);
			return {};
		}
		return json[key].get<bool>();
	}

	base::Optional<std::vector<nlohmann::json>> getArrayNoError(
		const nlohmann::json& json, const Key& key
	) {
		if (!json.contains(key) || !json[key].is_array()) return {};
		std::vector<nlohmann::json> result;
		result.reserve(json[key].size());
		for (const auto& elem: json[key]) result.push_back(elem);
		return result;
	}

	base::Optional<std::vector<nlohmann::json>> getArrayWarning(
		const nlohmann::json& json, const Key& key, const std::string& warning_message
	) {
		auto result = getArrayNoError(json, key);
		if (!result.has_value()) logFieldWarning("array", key, warning_message);
		return result;
	}

	base::Optional<std::vector<nlohmann::json>> getArray(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		auto result = getArrayNoError(json, key);
		if (!result.has_value()) logFieldError("array", key, error_message);
		return result;
	}

	base::Optional<nlohmann::json> getObjectNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_object()) return {};
		return json[key];
	}

	base::Optional<nlohmann::json> getObjectWarning(
		const nlohmann::json& json, const Key& key, const std::string& warning_message
	) {
		auto result = getObjectNoError(json, key);
		if (!result.has_value()) logFieldWarning("object", key, warning_message);
		return result;
	}

	base::Optional<nlohmann::json> getObject(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		auto result = getObjectNoError(json, key);
		if (!result.has_value()) logFieldError("object", key, error_message);
		return result;
	}

	base::Optional<nlohmann::json> getObjectIfPresent(
		const nlohmann::json& json, const Key& key, const std::string& error_message
	) {
		if (!json.contains(key)) return {};
		if (!json[key].is_object()) {
			logFieldError("object", key, error_message);
			return {};
		}
		return json[key];
	}

	bool checkIsObject(const nlohmann::json& json, const std::string& context_name) {
		if (json.is_object()) return true;
		if (global_state::hasGlobalLogger()) {
			global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
				base::strConcat(context_name, " must be a JSON object"),
				base::strConcat(
					"The value for ", context_name, " is not a JSON object and cannot be parsed."
				)
			));
		}
		return false;
	}

	base::Optional<base::StrID> getStringFromArrayNoError(const nlohmann::json& elem) {
		if (!elem.is_string()) return {};
		return base::StrID(elem.get<std::string>());
	}

	base::Optional<base::StrID> getStringFromArrayWarning(
		const nlohmann::json& elem, const std::string& parent_name
	) {
		auto result = getStringFromArrayNoError(elem);
		if (!result.has_value()) {
			logWarning(
				base::strConcat("Non-string value in array of ", parent_name),
				base::strConcat(
					"In ",
					parent_name,
					": a non-string value was found in the array and will be ignored."
				)
			);
		}
		return result;
	}

	base::Optional<base::StrID> getStringFromArray(
		const nlohmann::json& elem, const std::string& parent_name
	) {
		auto result = getStringFromArrayNoError(elem);
		if (!result.has_value() && global_state::hasGlobalLogger()) {
			global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Non-string value in array of ", parent_name),
				base::strConcat("In ", parent_name, ": a non-string value was found in the array.")
			));
		}
		return result;
	}

	void checkForUknownFields(
		const nlohmann::json&   json,
		const std::vector<Key>& required_keys,
		const std::vector<Key>& optional_keys,
		const std::string&      context
	) {
		if (!json.is_object()) CORE_PANIC("JSON value is not an object!");

		std::unordered_set<std::string> known_keys;
		for (const auto& k: required_keys) known_keys.insert(k);
		for (const auto& k: optional_keys) known_keys.insert(k);

		for (const auto& [key, _]: json.items()) {
			if (!known_keys.contains(key)) {
				logWarning(
					base::strConcat("Unknown field: ", key),
					base::strConcat(
						"In ",
						context,
						": field \"",
						key,
						"\" is not recognized and will be ignored."
					)
				);
			}
		}
	}

}  // namespace compiler::driver::json

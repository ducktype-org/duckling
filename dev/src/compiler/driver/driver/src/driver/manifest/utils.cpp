#include "utils.hpp"

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <json/json.hpp>

#include <unordered_set>

namespace compiler::driver::json {

	namespace {

		class ManifestWarning final : public dia_int::MessageBase {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "warning",
					     .family        = "misc",
					     .name          = "placeholder_header" };
			}

		public:
			ManifestWarning(std::string header, std::string description = "") : MessageBase() {
				addArgument<dia_int::TextArgument>("header_message", std::move(header));
				addArgument<dia_int::TextArgument>("description", std::move(description));
			}
		};

		void logFieldError(
			std::string_view     field_type,
			const std::string&   key,
			const std::string&   error_message
		) {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(
				makeBox<dia_int::PlaceholderHeaderError>(
					base::strConcat("No ", field_type, " value with key: ", key),
					error_message
				)
			);
		}

		void logWarning(const std::string& header, const std::string& description = "") {
			if (!global_state::hasGlobalLogger()) return;
			global_state::getGlobalLogger()->log(makeBox<ManifestWarning>(header, description));
		}

		void logFieldWarning(
			std::string_view     field_type,
			const std::string&   key,
			const std::string&   warning_message
		) {
			logWarning(
				base::strConcat("No ", field_type, " value with key: ", key),
				warning_message
			);
		}

	}  // namespace

	base::Optional<base::StrID> getStringNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_string()) {
			return {};
		}
		return base::StrID(json[key].get<std::string>());
	}

	base::Optional<base::StrID> getStringWarning(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    warning_message
	) {
		auto result = getStringNoError(json, key);
		if (!result.has_value()) {
			logFieldWarning("string", key, warning_message);
		}
		return result;
	}

	base::Optional<base::StrID> getString(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    error_message
	) {
		auto result = getStringNoError(json, key);
		if (!result.has_value()) {
			logFieldError("string", key, error_message);
		}
		return result;
	}

	base::Optional<bool> getBoolNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_boolean()) {
			return {};
		}
		return json[key].get<bool>();
	}

	base::Optional<bool> getBoolWarning(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    warning_message
	) {
		auto result = getBoolNoError(json, key);
		if (!result.has_value()) {
			logFieldWarning("bool", key, warning_message);
		}
		return result;
	}

	base::Optional<bool> getBool(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    error_message
	) {
		auto result = getBoolNoError(json, key);
		if (!result.has_value()) {
			logFieldError("bool", key, error_message);
		}
		return result;
	}

	base::Optional<std::vector<nlohmann::json>> getArrayNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_array()) {
			return {};
		}
		std::vector<nlohmann::json> result;
		result.reserve(json[key].size());
		for (const auto& elem : json[key]) {
			result.push_back(elem);
		}
		return result;
	}

	base::Optional<std::vector<nlohmann::json>> getArrayWarning(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    warning_message
	) {
		auto result = getArrayNoError(json, key);
		if (!result.has_value()) {
			logFieldWarning("array", key, warning_message);
		}
		return result;
	}

	base::Optional<std::vector<nlohmann::json>> getArray(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    error_message
	) {
		auto result = getArrayNoError(json, key);
		if (!result.has_value()) {
			logFieldError("array", key, error_message);
		}
		return result;
	}

	base::Optional<nlohmann::json> getObjectNoError(const nlohmann::json& json, const Key& key) {
		if (!json.contains(key) || !json[key].is_object()) {
			return {};
		}
		return json[key];
	}

	base::Optional<nlohmann::json> getObjectWarning(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    warning_message
	) {
		auto result = getObjectNoError(json, key);
		if (!result.has_value()) {
			logFieldWarning("object", key, warning_message);
		}
		return result;
	}

	base::Optional<nlohmann::json> getObject(
		const nlohmann::json& json,
		const Key&            key,
		const std::string&    error_message
	) {
		auto result = getObjectNoError(json, key);
		if (!result.has_value()) {
			logFieldError("object", key, error_message);
		}
		return result;
	}

	void checkForUknownFields(
		const nlohmann::json&   json,
		const std::vector<Key>& required_keys,
		const std::vector<Key>& optional_keys,
		const std::string&      context
	) {
		if (!json.is_object()){
			CORE_PANIC("JSON value is not an object!");
		}

		std::unordered_set<std::string> known_keys;
		for (const auto& k : required_keys) known_keys.insert(k);
		for (const auto& k : optional_keys) known_keys.insert(k);

		for (const auto& [key, _] : json.items()) {
			if (!known_keys.contains(key)) {
				logWarning(
					base::strConcat("Unknown field: ", key),
					base::strConcat("In ", context, ": field \"", key, "\" is not recognized and will be ignored.")
				);
			}
		}
	}

}  // namespace compiler::driver::json

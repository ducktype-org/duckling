#include "extract.hpp"

#include <unordered_set>

namespace js {

	namespace {
		std::string keyString(std::string_view key) { return std::string{ key }; }
	}

	bool isObject(const nlohmann::json& j) noexcept { return j.is_object(); }

	std::expected<base::StrID, JsonExtractError> extractString(
		const nlohmann::json& j, std::string_view key
	) {
		if (!j.is_object()) return std::unexpected(JsonExtractError::NotAnObject);
		const auto k = keyString(key);
		if (!j.contains(k)) return std::unexpected(JsonExtractError::MissingKey);
		if (!j.at(k).is_string()) return std::unexpected(JsonExtractError::WrongType);
		return base::StrID(j.at(k).get<std::string>());
	}

	std::expected<bool, JsonExtractError> extractBool(const nlohmann::json& j, std::string_view key) {
		if (!j.is_object()) return std::unexpected(JsonExtractError::NotAnObject);
		const auto k = keyString(key);
		if (!j.contains(k)) return std::unexpected(JsonExtractError::MissingKey);
		if (!j.at(k).is_boolean()) return std::unexpected(JsonExtractError::WrongType);
		return j.at(k).get<bool>();
	}

	std::expected<std::vector<nlohmann::json>, JsonExtractError> extractArray(
		const nlohmann::json& j, std::string_view key
	) {
		if (!j.is_object()) return std::unexpected(JsonExtractError::NotAnObject);
		const auto k = keyString(key);
		if (!j.contains(k)) return std::unexpected(JsonExtractError::MissingKey);
		const auto& node = j.at(k);
		if (!node.is_array()) return std::unexpected(JsonExtractError::WrongType);
		std::vector<nlohmann::json> result;
		result.reserve(node.size());
		for (const auto& elem: node) result.push_back(elem);
		return result;
	}

	std::expected<nlohmann::json, JsonExtractError> extractObject(
		const nlohmann::json& j, std::string_view key
	) {
		if (!j.is_object()) return std::unexpected(JsonExtractError::NotAnObject);
		const auto k = keyString(key);
		if (!j.contains(k)) return std::unexpected(JsonExtractError::MissingKey);
		if (!j.at(k).is_object()) return std::unexpected(JsonExtractError::WrongType);
		return j.at(k);
	}

	std::expected<base::StrID, JsonExtractError> extractStringValue(const nlohmann::json& j) {
		if (!j.is_string()) return std::unexpected(JsonExtractError::WrongType);
		return base::StrID(j.get<std::string>());
	}

	std::vector<std::string> findUnknownFields(
		const nlohmann::json&             j,
		std::span<const std::string_view> required,
		std::span<const std::string_view> optional
	) {
		std::vector<std::string> unknown;
		if (!j.is_object()) return unknown;

		std::unordered_set<std::string_view> known;
		known.reserve(required.size() + optional.size());
		for (auto k: required) known.insert(k);
		for (auto k: optional) known.insert(k);

		for (const auto& [key, _]: j.items())
			if (!known.contains(key)) unknown.push_back(key);

		return unknown;
	}

}  // namespace js

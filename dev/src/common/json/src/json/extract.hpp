/**
 * @file extract.hpp
 * @brief Pure, result-based helpers for extracting typed values from nlohmann::json.
 *
 * These helpers do not log anything. Each returns std::expected<T, JsonExtractError>
 * so callers can decide how to react to a failure (log, accumulate diagnostics, etc.).
 */
#pragma once

#include <string_id/string_id.hpp>

#include <nlohmann/json.hpp>

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace js {

	/**
	 * @brief Reason an extraction from a JSON object failed.
	 */
	enum class JsonExtractError {
		NotAnObject,  ///< the surrounding value is not a JSON object
		MissingKey,   ///< key is not present in the object
		WrongType,    ///< key is present but holds a value of an unexpected type
	};

	/**
	 * @brief True if the JSON value is an object.
	 */
	bool isObject(const nlohmann::json& j) noexcept;

	/**
	 * @brief Extract a string field from an object by key.
	 */
	std::expected<base::StrID, JsonExtractError> extractString(
		const nlohmann::json& j, std::string_view key
	);

	/**
	 * @brief Extract a boolean field from an object by key.
	 */
	std::expected<bool, JsonExtractError> extractBool(
		const nlohmann::json& j, std::string_view key
	);

	/**
	 * @brief Extract an array field as a vector of JSON nodes.
	 */
	std::expected<std::vector<nlohmann::json>, JsonExtractError> extractArray(
		const nlohmann::json& j, std::string_view key
	);

	/**
	 * @brief Extract an object field by key.
	 */
	std::expected<nlohmann::json, JsonExtractError> extractObject(
		const nlohmann::json& j, std::string_view key
	);

	/**
	 * @brief Interpret a JSON value (not an object) as a string.
	 * Returns WrongType if the value is not a string.
	 */
	std::expected<base::StrID, JsonExtractError> extractStringValue(const nlohmann::json& j);

	/**
	 * @brief List object keys that are not in the union of @p required and @p optional.
	 * @return Keys not in either set; empty if all keys are recognized or @p j is not an object.
	 */
	std::vector<std::string> findUnknownFields(
		const nlohmann::json&             j,
		std::span<const std::string_view> required,
		std::span<const std::string_view> optional
	);

}  // namespace js

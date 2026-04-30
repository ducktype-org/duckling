/**
 * @brief Utility functions for handling JSON data in the Duckling compiler driver.
 * These functions are used to parse JSON objects and extract relevant information for package compilation tasks and manifests.
 * Function in this file log errors to the global logger if the JSON data is invalid or missing required fields, and return an empty optional in such cases.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>
#include <vector>
#include "string_id/string_id.hpp"
#include <base/collections/optional.hpp>

namespace compiler::driver::json {

    /**
     * @brief A type alias for JSON keys.
     */
    using Key = std::string;

    /**
     * @brief Extracts a string value from a JSON object.
     * @param json The JSON object to extract from.
     * @param key The key of the value to extract.
     * @param error_message The error message to log if the key is missing or has an invalid type.
     * @return An optional containing the extracted string, or an empty optional if extraction fails.
     */
    base::Optional<base::StrID> getString(const nlohmann::json& json, const Key& key, const std::string& error_message);

    /**
     * @brief Extracts a boolean value from a JSON object.
     * @param json The JSON object to extract from.
     * @param key The key of the value to extract.
     * @param error_message The error message to log if the key is missing or has an invalid type.
     * @return An optional containing the extracted boolean, or an empty optional if extraction fails.
     */
    base::Optional<bool> getBool(const nlohmann::json& json, const Key& key, const std::string& error_message);

    /**
     * @brief Extracts an array value from a JSON object.
     * @param json The JSON object to extract from.
     * @param key The key of the value to extract.
     * @param error_message The error message to log if the key is missing or has an invalid type.
     * @return An optional containing the extracted array, or an empty optional if extraction fails.
     */
    base::Optional<std::vector<nlohmann::json>> getArray(const nlohmann::json& json, const Key& key, const std::string& error_message);

    /**
     * @brief Extracts an object value from a JSON object.
     * @param json The JSON object to extract from.
     * @param key The key of the value to extract.
     * @param error_message The error message to log if the key is missing or has an invalid type.
     * @return An optional containing the extracted object, or an empty optional if extraction fails.
     */
    base::Optional<nlohmann::json> getObject(const nlohmann::json& json, const Key& key, const std::string& error_message);

    /**
     * @brief Checks for unknown fields in a JSON object.
     * This function logs warning to the global logger for any fields that are not in the list of required or optional keys.
     * This function might br extended in the future.
     * @param json The JSON object to check.
     * @param required_keys The list of required keys.
     * @param optional_keys The list of optional keys.
     * @param parent_name The name of the parent object for error messages.
     */
    void checkForUknownFields(const nlohmann::json& json, const std::vector<Key>& required_keys, const std::vector<Key>& optional_keys, const std::string& parent_name);
}
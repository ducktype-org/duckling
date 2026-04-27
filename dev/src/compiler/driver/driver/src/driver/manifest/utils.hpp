#pragma once

#include <nlohmann/json_fwd.hpp>
#include <vector>
#include "string_id/string_id.hpp"
#include <base/collections/optional.hpp>

namespace compiler::driver::json {

    using Key = std::string;

    base::Optional<base::StrID> getString(const nlohmann::json& json, const Key& key, const std::string& error_message);

    base::Optional<bool> getBool(const nlohmann::json& json, const Key& key, const std::string& error_message);

    base::Optional<std::vector<nlohmann::json>> getArray(const nlohmann::json& json, const Key& key, const std::string& error_message);

    base::Optional<nlohmann::json> getObject(const nlohmann::json& json, const Key& key, const std::string& error_message);

    void verifyNumberOfFields(const nlohmann::json& json, const std::vector<Key>& required_keys, const std::vector<Key>& optional_keys, const std::string& error_message);
}
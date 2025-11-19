#pragma once
#include <yaml-cpp/yaml.h>

#include <base/box.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>

#include <json/json.hpp>

#include <expected>
#include <functional>

#define UNIMPLEMENTED() \
	do {                \
		assert(!"UNIMPLEMENTED function called! Or you just have to fix the code, because it didn't compile otherwise :p");          \
		std::abort();   \
	} while (0)

namespace dia_app {
	using json = nlohmann::json;

	struct TemplateFileNotFoundException: std::exception {};

#define ASSUME_OBJ(jf)      CORE_ASSERT(jf.is_object(), #jf " is not an object")
#define ASSUME_STR(jf, key) CORE_ASSERT(jf[key].is_string(), #jf "[ %s ] is not a string", key)
#define ASSUME_UINT(jf, key) \
	CORE_ASSERT(jf[key].is_number_unsigned(), #jf "[ %s ] is not an unsigned integer", key)
#define ASSUME_ARR(jf, key) CORE_ASSERT(jf[key].is_array(), #jf "[ %s ] is not an array", key)

#define ASSUME_HAS(jf, key) CORE_ASSERT(jf.contains(key), #jf " has no key %s", key)
#define ASSUME_HAS_STR(jf, key) \
	do {                        \
		ASSUME_HAS(jf, key);    \
		ASSUME_STR(jf, key);    \
	} while (false)
#define ASSUME_HAS_STR_ASSIGN(jf, key_var) \
	do {                                   \
		ASSUME_HAS_STR(jf, #key_var);      \
		key_var = jf[#key_var];            \
	} while (false)
#define ASSUME_HAS_UINT(jf, key) \
	do {                         \
		ASSUME_HAS(jf, key);     \
		ASSUME_UINT(jf, key);    \
	} while (false)
#define ASSUME_HAS_UINT_ASSIGN(jf, key_var) \
	do {                                    \
		ASSUME_HAS_UINT(jf, #key_var);      \
		key_var = jf[#key_var];             \
	} while (false)

#define ASSUME_VAL(jf, key, value)                         \
	CORE_ASSERT(                                           \
		jf[key] == value,                                  \
		"invalid " #key ", expected: %s but provided: %s", \
		value,                                             \
		std::string(jf[key])                               \
	)

#define YAML_ASSUME_HAS_SCALAR(node, key) \
	CORE_ASSERT(node[key] && node[key].IsScalar(), "YAML node missing scalar key: %s", key)

#define YAML_ASSUME_HAS(node, key) CORE_ASSERT(node[key], "YAML node missing key: %s", key)

	template<typename T>
	inline base::HashMap<std::string, T> yamlToMap(
		const YAML::Node& parent_node, const char* field_name
	) {
		base::HashMap<std::string, T> result;
		if (const auto map_node = parent_node[field_name]; map_node && map_node.IsMap()) {
			for (const auto& entry: map_node) {
				auto key = entry.first.as<std::string>();
				result.put(key, T::fromYaml(entry.second));
			}
		}
		return result;
	}

	// yamlToBox map
	template<typename T>
	inline base::HashMap<std::string, Box<T>> yamlToBoxMap(
		const YAML::Node& parent_node, const char* field_name
	) {
		base::HashMap<std::string, Box<T>> result;
		if (const auto map_node = parent_node[field_name]; map_node && map_node.IsMap()) {
			for (const auto& entry: map_node) {
				auto key = entry.first.as<std::string>();
				result.put(key, T::fromYaml(entry.second));
			}
		}
		return result;
	}

	/**
	 * @brief Conversion from a `json` element to a hash map with element
	 * transformation.
	 *
	 * @tparam V The hash map element type.
	 * @param data The `json` element to be converted.
	 * @param fun `The element transformer. Each element of the converted `json`
	 * is transformed by this function. Identity by default.
	 * @return base::HashMap<std::string, V>
	 */
	template<typename V>
	base::HashMap<std::string, V> jsonToMap(
		const json& data, std::function<V(const json&)> fun = [](const json& el) { return el; }
	) {
		ASSUME_OBJ(data);
		base::HashMap<std::string, V> res;
		for (auto& [key, val]: data.items()) res.put(key, fun(val));
		return res;
	}


}

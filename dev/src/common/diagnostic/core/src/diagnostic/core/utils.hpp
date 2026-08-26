/**
 * @file utils.hpp
 * @author Wojciech Rzeplinski
 * @brief Utility macros and functions for working with yaml and json deserialization.
 *
 */
#pragma once
#include "exceptions.hpp"

#include <yaml-cpp/yaml.h>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <json/json.hpp>

#include <expected>
#include <functional>

#define UNIMPLEMENTED() \
	do {                \
		assert(!"UNIMPLEMENTED function called! Or you just have to fix the code, because it didn't compile otherwise :p");          \
		std::abort();   \
	} while (0)

namespace dia {
	using json = nlohmann::json;

#define ASSUME_OBJ(jf) \
	if (!jf.is_object()) throw ParsingDiagnosticFileError(#jf " is not an object")
#define ASSUME_STR(jf, key)   \
	if (!jf[key].is_string()) \
	throw ParsingDiagnosticFileError(base::strConcat(#jf "[ ", key, " ] is not a string"))
#define ASSUME_UINT(jf, key)                                            \
	if (!jf[key].is_number_unsigned())                                  \
	throw ParsingDiagnosticFileError(                                   \
		base::strConcat(#jf "[ ", key, " ] is not an unsigned integer") \
	)
#define ASSUME_ARR(jf, key)  \
	if (!jf[key].is_array()) \
	throw ParsingDiagnosticFileError(base::strConcat(#jf "[ ", key, " ] is not an array"))

#define ASSUME_HAS(jf, key) \
	if (!jf.contains(key))  \
	throw ParsingDiagnosticFileError(base::strConcat(#jf " has no key ", key))
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

#define ASSUME_VAL(jf, key, value)                                                       \
	if (jf[key] != value)                                                                \
	throw ParsingDiagnosticFileError(base::strConcat(                                    \
		"invalid ", #key, ", expected: ", value, " but provided: ", std::string(jf[key]) \
	))

#define YAML_ASSUME_HAS_SCALAR(node, key)    \
	if (!node[key] || !node[key].IsScalar()) \
	throw ParsingTemplateFileError(base::strConcat("YAML node missing scalar key: ", key))

#define YAML_ASSUME_HAS(node, key) \
	if (!node[key]) throw ParsingTemplateFileError(base::strConcat("YAML node missing key: ", key))

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

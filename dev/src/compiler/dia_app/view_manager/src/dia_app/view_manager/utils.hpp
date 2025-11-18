#pragma once
#include <yaml-cpp/yaml.h>

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/variant.hpp>
#include <base/box.hpp>
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

	/**
	 * @brief An info type.
	 *
	 * Each type suggests a different role the info plays in the diagnostic
	 * message.
	 *
	 * Type data can be used by the UI to visually distinguish infos containing
	 * different kinds of information. It also serves for identification
	 * of info templates.
	 *
	 */
	enum class InfoType { Error, Warning, Note, Hint, Docs };

	// `std::string` to `InfoType` conversion. Throws upon failure.
	inline InfoType from_string(const std::string& s) {
		if (s == "error") {
			return InfoType::Error;
		} else if (s == "warning") {
			return InfoType::Warning;
		} else if (s == "note") {
			return InfoType::Note;
		} else if (s == "hint") {
			return InfoType::Hint;
		} else if (s == "docs") {
			return InfoType::Docs;
		} else {
			CORE_ASSERT(
				false, "info type must be one of: error|warning|note|hint|docs, but provided: %s", s
			);
			return InfoType::Error;
		}
	}

	// Standardized `InfoType` to `std::string` conversion.
	inline std::string to_string(InfoType type) {
		switch (type) {
		case InfoType::Error:
			return "error";
		case InfoType::Warning:
			return "warning";
		case InfoType::Note:
			return "note";
		case InfoType::Hint:
			return "hint";
		case InfoType::Docs:
			return "docs";
		}
	}

	template<typename T>
	inline base::HashMap<std::string, T> yamlToMap(
		const YAML::Node& parent_node, const char* field_name
	) {
		base::HashMap<std::string, T> result;
		if (const auto map_node = parent_node[field_name]; map_node && map_node.IsMap()) {
			for (const auto& entry: map_node) {
				auto key = entry.first.as<std::string>();
				result.put(key, T::fromYaml(key, entry.second));
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

	// Info identifier. Enables the laziness mechanism for infos.
	using InfoID = std::string;
	// Entity identifier. Enables the laziness mechanism for entities.
	using EntityID = std::string;
	// Lazy display element identifier. Enables the laziness mechanism for
	// display elements.
	using LazyDisplayID = std::string;

	/**
	 * @brief Info metadata.
	 *
	 * Identifies an info template.
	 *
	 * Appears both in the diagnostic file as well as in info template
	 * files (in the `include ... on ...` template element).
	 *
	 * Each info template is identified by three components:
	 *
	 * 1. `type` (suggesting a general purpose of the info),
	 *
	 * 2. `family` (describing a broad category of infos within a given type),
	 *
	 * 3. `name` (identifying the info within said category).
	 *
	 * Their structure is reflected in the file structure of info templates.
	 */
	struct ShortMetadata {
		InfoType    type;
		std::string family;
		std::string name;

		ShortMetadata();
		// Parse `ShortMetadata` from a `json` element.
		ShortMetadata(const json& metadata);
		// Parse `ShortMetadata` from a `YAML` node.
		ShortMetadata(const YAML::Node& node);

		// Get a filepath of the info template identified by this metadata.
		std::string getPath() const;

		bool operator==(const ShortMetadata& other) const;
	};

	/**
	 * @brief Info metadata with additional information.
	 *
	 * An extension of `ShortMetadata` including a short info code
	 * and a range of versions this info is (or was) supported.
	 *
	 * Appears only in the info template files.
	 */
	struct Metadata {
		InfoType    type;
		std::string family;
		std::string name;
		u32         code;
		std::string active_from;
		std::string active_until;

		Metadata();
		// Parse `Metadata` from a `YAML` node.
		Metadata(const YAML::Node& node);

		// Compare with `ShortMetadata`. Return true if both identify
		// the same info template.
		bool sameAs(const ShortMetadata& metadata) const;
	};

	/**
	 * @brief Check if the `case ... of ...` key represents an exact match or
	 * a class match.
	 *
	 * Class matches can be identified as they begin with '[' and end with ']'.
	 *
	 * @param key The key to be identified.
	 * @return true The key represents an exact match.
	 * @return false The key represents a class match.
	 *
	 */
	inline bool is_case_exact(const std::string& key) {
		return key.size() == 0 || (key[0] != '[' && key[key.size() - 1] != ']');
	}

}

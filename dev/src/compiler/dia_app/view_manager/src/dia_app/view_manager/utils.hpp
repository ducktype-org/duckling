#pragma once
#include <iostream>
#include <expected>
#include <functional>
#include <json/json.hpp>
#include <yaml-cpp/yaml.h>
#include <base/optional.hpp>
#include <base/maps.hpp>
#include <base/variant.hpp>

#ifdef DEBUG
auto&operator<<(auto&o,std::pair<auto,auto>p){return o<<"("<<p.first<<", "<<p.second<<")";}
auto operator<<(auto&o,auto x)->decltype(x.end(),o){o<<"{";int i=0;for(auto e:x)o<<","+!i++<<e;return o<<"}";}
#define debug(X...)std::cerr<<"["#X"]: ",[](auto...$){((std::cerr<<$<<"; "),...)<<std::endl;}(X)
#else
#define debug(...){}
#endif

namespace dia_app {
    using json = nlohmann::json;

    struct TemplateFileNotFoundException : std::exception {};

    #ifndef NDEBUG
    #   define ASSERT(condition, message) \
        do { \
            if (! (condition)) { \
                std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                        << " line " << __LINE__ << ": " << message << std::endl; \
                std::terminate(); \
            } \
        } while (false)
    #else
    #   define ASSERT(condition, message) do { } while (false)
    #endif

    #define ASSUME(expr, msg) ASSERT(expr, msg)
    #define ASSUME_OBJ(jf) ASSUME(jf.is_object(), #jf " is not an object")
    #define ASSUME_STR(jf, key) ASSUME(jf[key].is_string(), #jf "[" << key << "] is not a string")
    #define ASSUME_UINT(jf, key) ASSUME(jf[key].is_number_unsigned(), #jf "[" << key << "] is not an unsigned integer")
    #define ASSUME_ARR(jf, key) ASSUME(jf[key].is_array(), #jf " is not an array")

    #define ASSUME_HAS(jf, key) ASSUME(jf.contains(key), #jf " has no key " << key)
    #define ASSUME_HAS_STR(jf, key) do { ASSUME_HAS(jf, key); ASSUME_STR(jf, key); } while(false)
    #define ASSUME_HAS_STR_ASSIGN(jf, key_var) do { ASSUME_HAS_STR(jf, #key_var); key_var = jf[#key_var]; } while(false)
    #define ASSUME_HAS_UINT(jf, key)  do { ASSUME_HAS(jf, key); ASSUME_UINT(jf, key); } while(false)
    #define ASSUME_HAS_UINT_ASSIGN(jf, key_var) do { ASSUME_HAS_UINT(jf, #key_var); key_var = jf[#key_var]; } while(false)

    #define ASSUME_VAL(jf, key, value) ASSUME(jf[key] == value, "invalid " #key ", expected: " << value << " but provided: " << jf[key])
    #define ASSUME_MSG(jf) ASSUME(is_message(jf), #jf " is not a message")
    #define ASSUME_SUBST(jf, data) ASSUME(substitute_params(jf, data), "parameter substitution failed for " #jf)

    // Location of message templates.
    extern std::string MESSAGE_TEMPLATE_PATH;

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
    enum class InfoType {
        Error,
        Warning,
        Note,
        Hint,
        Docs
    };

    // `std::string` to `InfoType` conversion. Throws upon failure.
    inline InfoType from_string(const std::string &s) {
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
            ASSUME(false, "info type must be one of: error|warning|note|hint|docs, but provided: " << s);
            return InfoType::Error;
        }
    }

    // Standardized `InfoType` to `std::string` conversion.
    inline std::string to_string(InfoType type) {
        switch (type) {
            case InfoType::Error: return "error";
            case InfoType::Warning: return "warning";
            case InfoType::Note: return "note";
            case InfoType::Hint: return "hint";
            case InfoType::Docs: return "docs";
        }
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
    base::HashMap<std::string, V> from_json(const json& data, std::function<V(json)> fun = [](const json &el) { return el; }) {
        ASSUME_OBJ(data);
        base::HashMap<std::string, V> res;
        for (auto &[key, val] : data.items()) {
            res.put(key, fun(val));
        }
        return res;
    }
    
    // Info identifier. Enables the laziness mechanism for infos.
    using InfoID = u32;
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
        InfoType type;
        std::string family;
        std::string name;

        ShortMetadata() {}
        // Parse `ShortMetadata` from a `json` element.
        ShortMetadata(const json &metadata);
        // Parse `ShortMetadata` from a `YAML` node.
        ShortMetadata(const YAML::Node &node);

        // Get a filepath of the info template identified by this metadata.
        std::string getPath() const;

        bool operator==(const ShortMetadata &other) const;
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
        InfoType type;
        std::string family;
        std::string name;
        u32 code;
        std::string active_from;
        std::string active_until;

        Metadata() {}
        // Parse `Metadata` from a `YAML` node.
        Metadata(const YAML::Node &node);

        // Compare with `ShortMetadata`. Return true if both identify
        // the same info template.
        bool sameAs(const ShortMetadata &metadata) const;
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
    inline bool is_case_exact(const std::string &key) {
        return key.size() == 0 || (key[0] != '[' && key[key.size() - 1] != ']');
    }

}
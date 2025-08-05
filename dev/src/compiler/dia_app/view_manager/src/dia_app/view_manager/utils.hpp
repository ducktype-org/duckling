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

    /* Location of message templates */
    extern std::string MESSAGE_TEMPLATE_PATH;

    enum class InfoType {
        Error,
        Warning,
        Note,
        Hint,
        Docs
    };

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

    inline std::string to_string(InfoType type) {
        switch (type) {
            case InfoType::Error: return "error";
            case InfoType::Warning: return "warning";
            case InfoType::Note: return "note";
            case InfoType::Hint: return "hint";
            case InfoType::Docs: return "docs";
        }
    }

    template<typename V>
    base::HashMap<std::string, V> from_json(const json& data, std::function<V(json)> fun = [](const json &el) { return el; }) {
        ASSUME_OBJ(data);
        base::HashMap<std::string, V> res;
        for (auto &[key, val] : data.items()) {
            res.put(key, fun(val));
        }
        return res;
    }
    
    // Message params metadata (identifies the message template).
    // Appears both in the diagnostic file and in message template files (as "include on").
    struct ShortMetadata {
        InfoType type;
        std::string family;
        std::string name;

        ShortMetadata() {}
        ShortMetadata(const json &metadata);
        ShortMetadata(const YAML::Node &node);

        std::string getPath() const;

        bool operator==(const ShortMetadata &other) const;
    };

    // Message template metadata.
    // Appears only in the message template files.
    struct Metadata {
        InfoType type;
        std::string family;
        std::string name;
        u32 code;
        std::string active_from;
        std::string active_until;

        Metadata() {}
        Metadata(const YAML::Node &node);

        bool sameAs(const ShortMetadata &metadata) const;
    };

    // Check if the "case of" key is an exact match (true) or a class match (false).
    inline bool is_case_exact(const std::string &key) {
        return key.size() == 0 || (key[0] != '[' && key[key.size() - 1] != ']');
    }

    namespace message_template {
        struct TemplateData;
    }
    namespace dia_file {
        struct InfoParams;
        struct DisplayElement;
        using DisplayPtr = std::shared_ptr<DisplayElement>;
    }

    using InfoID = u32;
    using EntityID = std::string;
    using LazyDisplayID = std::string;

    struct ViewConstructor;
}
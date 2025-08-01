#pragma once
#include <base/optional.hpp>
#include <base/maps.hpp>

#include <iostream>
#include <expected>
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
    using cstrr = const std::string &;

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
    base::HashMap<std::string, V> from_json(const json& data) {
        ASSUME_OBJ(data);
        base::HashMap<std::string, V> res;
        for (auto &[key, val] : data.items()) {
            res.put(key, val);
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
        ShortMetadata(const json &metadata) {
            ASSUME_HAS_STR(metadata, "type");
            type = from_string(metadata["type"]);
            ASSUME_HAS_STR_ASSIGN(metadata, family);
            ASSUME_HAS_STR_ASSIGN(metadata, name);
        }

        ShortMetadata(const YAML::Node &node) {
            // Required fields
            assert(node["type"] && node["type"].IsScalar());
            assert(node["family"] && node["family"].IsScalar());
            assert(node["name"] && node["name"].IsScalar());

            type = from_string(node["type"].as<std::string>());
            family = node["family"].as<std::string>();
            name = node["name"].as<std::string>();
        }

        std::string getPath() const {
            return MESSAGE_TEMPLATE_PATH + to_string(type) + '/' + family + '/' + name + ".yaml";
        }

        bool operator==(const ShortMetadata &other) const {
            return type == other.type && family == other.family && name == other.name;
        }
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
        Metadata(const YAML::Node &node) {
            // Required fields
            assert(node["type"] && node["type"].IsScalar());
            assert(node["family"] && node["family"].IsScalar());
            assert(node["name"] && node["name"].IsScalar());
            assert(node["code"] && node["code"].IsScalar());
            assert(node["active_from"] && node["active_from"].IsScalar());
            assert(node["active_until"] && node["active_until"].IsScalar());

            type = from_string(node["type"].as<std::string>());
            family = node["family"].as<std::string>();
            name = node["name"].as<std::string>();
            try {
                code = std::stoi(node["code"].as<std::string>());
            } catch (const std::logic_error &e) {
                ASSUME(false, "message code must be convertible to an integer, instead provided: " << node["code"].as<std::string>());
            }
            active_from = node["active_from"].as<std::string>();
            active_until = node["active_until"].as<std::string>();
        }

        bool sameAs(const ShortMetadata &metadata) const {
            return type == metadata.type
                && family == metadata.family
                && name == metadata.name;
        }
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

    using ComponentHandle = json;

    // An explore edge for the state manager (with evaluated description).
    struct ExploreEdge {
        std::string description;
        InfoID handle;

        ExploreEdge(const std::string &description, InfoID handle) :
            description(description), handle(handle) {}
    };
    // An explore edge from the diagnostic file.
    struct ExploreEdgeParams {
        std::string name;
        base::HashMap<std::string, dia_file::DisplayPtr> params;
        InfoID handle;

        ExploreEdgeParams(const json &edge);
    };

    struct ViewConstructor;

    // A data handle for accessing and modifying the entities, secondary_infos,
    // and a specific message template of a view constructor instance.
    // 
    // Note: this handle contains references to ViewConstructor fields.
    //       Do not let it escape the ViewConstructor's scope!
    struct TemplateDataHandle {
        using TemplateData = message_template::TemplateData;
        using InfoParams = dia_file::InfoParams;

        ViewConstructor &vc;
        const TemplateData &template_data;
        // Auxiliary parameters for explore edges templates (shadow param_data).
        base::HashMap<std::string, dia_file::DisplayPtr> aux_params;
        const InfoParams &param_data;

        // Macro evaluation stack for detecting infinite recursion.
        std::set<std::string> macro_stack;

        TemplateDataHandle(
            ViewConstructor &vc,
            const TemplateData &template_data,
            const InfoParams &param_data
        );
        TemplateDataHandle with_aux_params(const base::HashMap<std::string, dia_file::DisplayPtr> &aux_params) const;

    };
}
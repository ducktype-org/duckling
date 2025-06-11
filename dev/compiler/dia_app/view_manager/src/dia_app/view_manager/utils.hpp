#pragma once
#include <iostream>
#include <expected>
#include <json/json.hpp>
#include <yaml-cpp/yaml.h>

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
        uint code;
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
        struct ParamData;
        struct DisplayElement;
        using Ptr = std::shared_ptr<DisplayElement>;
    }
    // A stateless info params handle for lazy fetching of infos.
    using InfoHandle = uint;
    struct InfoParamsHandle;

    // An explore edge for the state manager (with evaluated description).
    struct ExploreEdge {
        std::string description;
        InfoHandle handle;

        ExploreEdge(const std::string &description, InfoHandle handle) :
            description(description), handle(handle) {}
    };
    // An explore edge from the diagnostic file.
    struct ExploreEdgeParams {
        std::string name;
        std::map<std::string, dia_file::Ptr> params;
        InfoHandle handle;

        ExploreEdgeParams(const json &edge);
    };

    // A data handle for accessing and modifying the entities and secondary_infos
    // of a view constructor instance.
    // 
    // Note: this handle contains references to ViewConstructor fields.
    //       Do not let it escape the ViewConstructor's scope!
    struct DataHandle {
        using ParamData = dia_file::ParamData;

        std::map<std::string, json> &entities;
        std::vector<ParamData> &secondary_infos;
        std::map<InfoHandle, InfoParamsHandle> &info_handles;

        DataHandle(
            std::map<std::string, json> &entities,
            std::vector<ParamData> &secondary_infos,
            std::map<InfoHandle, InfoParamsHandle> &info_handles
        );
    };

    // A data handle for accessing and modifying the entities, secondary_infos,
    // and a specific message template of a view constructor instance.
    // 
    // Note: this handle contains references to ViewConstructor fields.
    //       Do not let it escape the ViewConstructor's scope!
    struct TemplateDataHandle {
        using TemplateData = message_template::TemplateData;
        using ParamData = dia_file::ParamData;

        const TemplateData &template_data;
        // Auxiliary parameters for explore edges templates (shadow param_data).
        std::map<std::string, dia_file::Ptr> aux_params;
        const ParamData &param_data;

        std::map<std::string, json> &entities;
        std::vector<ParamData> &secondary_infos;
        std::map<InfoHandle, InfoParamsHandle> &info_handles;

        TemplateDataHandle(
            const TemplateData &template_data,
            const ParamData &param_data,
            DataHandle data_handle
        );
        TemplateDataHandle with_aux_params(const std::map<std::string, dia_file::Ptr> &aux_params) const;

        DataHandle toDataHandle() const;

    };

    // A handle for the lazy evaluation of alt_content.
    // To be replaced by some LS-generated handle for later access.
    using ResourceHandle = json;
    using EntityHandle = std::pair<std::string, json>;

    // A stateful info params handle for lazy fetching of infos.
    class InfoParamsHandle {
    private:
        std::optional<ShortMetadata> metadata;
        std::optional<uint> idx;
        
        // To be replaced by some LS-generated handle for later access.
        std::optional<json> param_data;

        uint load(DataHandle dh);
    
    public:
        InfoParamsHandle() {}
        InfoParamsHandle(uint idx) : idx(idx) {}
        InfoParamsHandle(const ShortMetadata &metadata) : metadata(metadata) {}
        InfoParamsHandle(const json &handle_json);
        
        bool operator==(const InfoParamsHandle &other) const {
            // InfoParamsHandle is always in one of three states.
            if (metadata.has_value()) {
                return other.metadata.has_value() && metadata.value() == other.metadata.value();
            }
            if (idx.has_value()) {
                return other.idx.has_value() && idx.value() == other.idx.value();
            }
            return param_data.has_value() && other.param_data.has_value() && param_data.value() == other.param_data.value();
        }

        // Fetch the info params for this handle if necessary.
        // The handle is updated upon fetching and so
        // does not fetch the data twice.
        static uint load(InfoHandle info, DataHandle dh);

        static InfoHandle add(const InfoParamsHandle &params_handle, DataHandle dh);
    };
}
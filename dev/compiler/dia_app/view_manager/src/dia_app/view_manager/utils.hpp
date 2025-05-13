#pragma once
#include <iostream>
#include <algorithm>
#include <expected>
#include <set>
#include <json/json.hpp>
#include <yaml-cpp/yaml.h>

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

    /* An empty json object which can be referenced */
    extern json EMPTY_OBJ;
    inline const json &or_empty(const json &jf, cstrr key) {
        ASSUME_OBJ(jf);
        return jf.contains(key) ? jf[key] : EMPTY_OBJ;
    }

    /* Location of message templates */
    extern std::string MESSAGE_TEMPLATE_PATH;
    
    // Message params metadata (identifies the message template)
    struct ShortMetadata {
        std::string type;
        std::string family;
        std::string name;

        ShortMetadata() {}
        ShortMetadata(const json &metadata) {
            ASSUME_HAS_STR_ASSIGN(metadata, type);
            ASSUME_HAS_STR_ASSIGN(metadata, family);
            ASSUME_HAS_STR_ASSIGN(metadata, name);
        }

        ShortMetadata(const YAML::Node &node) {
            // Required fields
            assert(node["type"] && node["type"].IsScalar());
            assert(node["family"] && node["family"].IsScalar());
            assert(node["name"] && node["name"].IsScalar());

            type = node["type"].as<std::string>();
            family = node["family"].as<std::string>();
            name = node["name"].as<std::string>();
        }

        std::string get_path() const {
            return MESSAGE_TEMPLATE_PATH + type + '/' + family + '/' + name + ".json";
        }

        bool operator==(const ShortMetadata &other) const {
            return type == other.type && family == other.family && name == other.name;
        }
    };

    // Message template metadata.
    struct Metadata {
        std::string type;
        std::string family;
        std::string name;
        std::string code;
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

            type = node["type"].as<std::string>();
            family = node["family"].as<std::string>();
            name = node["name"].as<std::string>();
            code = node["code"].as<std::string>();
            active_from = node["active_from"].as<std::string>();
            active_until = node["active_until"].as<std::string>();
        }

        bool same_as(const ShortMetadata &metadata) const {
            return type == metadata.type
                && family == metadata.family
                && name == metadata.name;
        }
    };

    inline bool is_case_exact(const std::string &key) {
        return key.size() == 0 || (key[0] != '[' && key[key.size() - 1] != ']');
    }

    namespace message_template {
        struct TemplateData;
    }
    namespace dia_file {
        struct ParamData;
    }
    // A stateless info params handle for lazy fetching of infos.
    using InfoHandle = uint;
    struct InfoParamsHandle;

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
        const ParamData &param_data;
        std::map<std::string, json> &entities;
        std::vector<ParamData> &secondary_infos;
        std::map<InfoHandle, InfoParamsHandle> &info_handles;

        TemplateDataHandle(
            const TemplateData &template_data,
            const ParamData &param_data,
            DataHandle data_handle
        );

        DataHandle to_data_handle() const;
    };

    // A handle for the lazy evaluation of alt_content.
    // To be replaced by some LS-generated handle for later access.
    using ResourceHandle = json;
    using EntityHandle = std::pair<std::string, json>;

    // A stateful info params handle for lazy fetching of infos.
    struct InfoParamsHandle {
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
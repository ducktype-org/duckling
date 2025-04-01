#pragma once
#include <iostream>
#include <fstream>
#include <algorithm>
#include <expected>
#include <json/json.hpp>

namespace dia_app {
    namespace message_template {

        using json = nlohmann::json;
        using cstrr = const std::string &;
    
        /* Location of message templates */
        const std::string template_location = "../compiler/dia_app/view_manager/src/dia_app/view_manager/templates/";
    
        /* An empty json object which can be referenced */
        const json EMPTY_OBJ = json::object_t();

        #define ASSUME(expr, msg) if (!(expr)) { std::cerr << msg << std::endl; return false; }
        #define ASSUME_OBJ(jf) ASSUME(jf.is_object(), #jf " is not an object")
        #define ASSUME_HAS(jf, key) ASSUME(jf.contains(key), #jf " has no key " << key)
        #define ASSUME_VAL(jf, key, value) ASSUME(jf[key] == value, "invalid " #key ", expected: " << jf[key] << " but provided: " << value)
        #define ASSUME_MSG(jf) ASSUME(is_message(jf), #jf " is not a message")
        #define ASSUME_SUBST(jf, data) ASSUME(substitute_params(jf, data), "parameter substitution failed for " #jf)
        
        const json &or_empty(const json &jf, cstrr key) {
            return jf.contains(key) ? jf[key] : EMPTY_OBJ;
        }
        
        /* Order of message type evaluation:
            string -> array -> parameter -> macro -> case of */
        bool is_message(const json &jf);
        
        bool is_text(const json &jf) {
            return jf.is_string()
                || (
                    jf.is_object()
                 && jf.contains("text")
                 && jf["text"].is_string()
            );
        }
        
        std::string get_text(const json &jf) {
            assert(is_text(jf));
            if (jf.is_string()) {
                return jf;
            } else {
                return jf["text"];
            }
        }
        
        bool is_param(const json &jf) {
            return jf.is_object()
                && jf.contains("param")
                && jf["param"].is_string();
        }
        
        bool is_macro(const json &jf) {
            return jf.is_object()
                && jf.contains("macro")
                && jf["macro"].is_string();
        }
        
        bool is_case_of(const json &jf) {
            return jf.is_object()
                && jf.contains("case")
                && jf.contains("of")
                && is_message(jf["case"])
                && jf["of"].is_object()
                && jf["of"].contains("[other]")
                && std::all_of(jf["of"].begin(), jf["of"].end(), is_message);
        }
        
        bool is_message(const json &jf) {
            return jf.is_string()
                || (jf.is_array() && std::all_of(jf.begin(), jf.end(), is_message))
                || is_param(jf)
                || is_macro(jf)
                || is_case_of(jf);
        }
        
        bool verify_metadata(const json &metadata, cstrr type, cstrr family, cstrr name) {
            ASSUME_HAS(metadata, "type");
            ASSUME_HAS(metadata, "name");
            ASSUME_HAS(metadata, "family");
            ASSUME_HAS(metadata, "code");
            ASSUME_HAS(metadata, "active_from");
            ASSUME_HAS(metadata, "active_until");
        
            ASSUME_VAL(metadata, "type", type);
            ASSUME_VAL(metadata, "family", family);
            ASSUME_VAL(metadata, "name", name);
        
            return true;
        }
        
        bool verify_params(const json &expected, const json &provided) {
            ASSUME_OBJ(expected);
            ASSUME_OBJ(provided);
            for (auto &[key, val] : expected.items()) {
                if (!val.contains("optional") || val["optional"] != true) {
                    ASSUME_HAS(provided, key);
                    // Note: the provided parameter needs not be a well-formed
                    // message, so we do not check against it.
                }
            }
            return true;
        }
        
        bool verify_macros(const json &macros) {
            ASSUME_OBJ(macros);
            ASSUME(std::all_of(macros.begin(), macros.end(), is_message), "not all macros are well-formed messages");
            return true;
        }
        
        struct TemplateData {
            const json &params;
            const json &declared_params;
            const json &macros;
        
            TemplateData(const json &params, const json &declared_params, const json &macros) :
                params(params),
                declared_params(declared_params),
                macros(macros) {}
        };

        bool verify_message_data(const json &data) {
            ASSUME_OBJ(data);
            ASSUME_HAS(data, "metadata");
        
            const json &metadata = data["metadata"];
            ASSUME_OBJ(metadata);
        
            ASSUME_HAS(metadata, "type");
            ASSUME(metadata["type"].is_string(), "metadata type is not a string");
        
            ASSUME_HAS(metadata, "family");
            ASSUME(metadata["family"].is_string(), "metadata family is not a string");
        
            ASSUME_HAS(metadata, "name");
            ASSUME(metadata["name"].is_string(), "metadata name is not a string");
        
            ASSUME_HAS(data, "params");
            ASSUME_OBJ(data["params"]);
        
            return true;
        }
        
        /* Assumption: message is well-formed. */
        bool substitute_params(json &message, const TemplateData &data) {
            if (is_text(message)) {
                return true;
            }
            
            if (message.is_array()) {
                return std::all_of(message.begin(), message.end(), [&data](json &el) {
                    return substitute_params(el, data);
                });
            }
            
            if (message.contains("param")) {
                auto &name = message["param"];
                ASSUME(data.declared_params.contains(name), "parameter " << name << " was used but not declared");
                ASSUME(data.params.contains(name), "parameter " << name << " was used but not provided");
                message = data.params[name];
                return true;
            }
        
            if (message.contains("macro")) {
                // Macro name lookup.
                auto &name = message["macro"];
                ASSUME_HAS(data.macros, name);
                // Replacement and recursive substitution.
                message = data.macros[name];
                return substitute_params(message, data);
            }
        
            if (message.contains("case")) {
                // Argument evaluation (evaluated argumenet will be stored in-place).
                ASSUME_SUBST(message["case"], data);
                // Argument match.
                if (is_text(message["case"]) && message["of"].contains(get_text(message["case"]))) {
                    auto &matched = message["of"][get_text(message["case"])];
                    // Matched expression evaluation and recursive substitution.
                    ASSUME_SUBST(matched, data);
                    message = matched;
                    return true;
                } else {
                    auto &matched = message["of"]["[other]"];
                    // Matched expression evaluation and recursive substitution.
                    ASSUME_SUBST(matched, data);
                    message = matched;
                    return true;
                }
            }
        
            return false;
        }
        
        bool verify_template(const json &metadata, const json &params, json &jf) {
            cstrr type = metadata["type"];
            cstrr family = metadata["family"];
            cstrr name = metadata["name"];
        
            std::string filename = template_location + type + '/' + family + '/' + name + ".json";
            std::ifstream file(filename);
        
            if (!file.is_open()) {
                std::cerr << "template file " << filename << " not found" << std::endl;
                return false;
            }
            jf = json::parse(file);
        
            ASSUME_HAS(jf, "metadata");
            ASSUME(verify_metadata(jf["metadata"], type, family, name), "metadata verification failed");
            
            const json &declared_params = or_empty(jf, "params");
            ASSUME(verify_params(declared_params, params), "parameter verification failed");
        
            const json &macros = or_empty(jf, "macros");
            ASSUME(verify_macros(macros), "macro verification failed");
            
            TemplateData data(params, declared_params, macros);
        
            ASSUME_HAS(jf, "header_message");
            ASSUME_MSG(jf["header_message"]);
            ASSUME_SUBST(jf["header_message"], data);
        
            if (jf.contains("description_message")) {
                ASSUME_MSG(jf["description_message"]);
                ASSUME_SUBST(jf["description_message"], data);
            }
        
            if (jf.contains("pointer_messages")) {
                auto &ptr_msgs = jf["pointer_messages"];
                ASSUME_OBJ(ptr_msgs);
                ASSUME(std::all_of(ptr_msgs.begin(), ptr_msgs.end(), is_message), "not all pointer messages are well-formed messages");
                ASSUME(std::all_of(ptr_msgs.begin(), ptr_msgs.end(), [&data](json &el) {
                    ASSUME_SUBST(el, data);
                    return true;
                }), "parameter substitution failed for some element(s)");
            }
        
            return true;
        }
        
        enum class Error {
            BadMessageData,
            BadTemplate
        };
        
        /*
            Apply a message template identified and parametrized by `data`.
        */
        std::expected<json, Error> apply(const json &data) {
            if (!verify_message_data(data)) {
                std::cerr << "message data invalid, provided:\n" << data.dump(2) << std::endl;
                return std::unexpected(Error::BadMessageData);
            }
        
            json result;
            if (!verify_template(data["metadata"], data["params"], result)) {
                std::cerr << "template application failed, resulting json:\n" << result.dump(2) << std::endl;
                return std::unexpected(Error::BadTemplate);
            }
            
            return result;
        }
    }
}

/* Example use of the `dia_app::message_template::apply` utility function. */
void example_use() {
    using json = nlohmann::json;

    std::ifstream data_file(dia_app::message_template::template_location + "../samples/message_data.json");
    json data = json::parse(data_file);

    if (auto result = dia_app::message_template::apply(data); result.has_value()) {
        std::cout << result.value().dump(2) << std::endl;
    } else {
        throw result.error();
    }
    
}
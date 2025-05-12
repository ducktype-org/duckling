#pragma once
#include <fstream>
#include "utils.hpp"
#include "dia_parser.hpp"

namespace dia_app {
namespace message_template {

    struct TemplateElement;
    using Ptr = std::shared_ptr<TemplateElement>;
    Ptr parse(const json &msg);

    #define MAX_PRIORITY_UINT 1000000000
    struct PointerMessage {
        // The lower this number, the higher the priority.
        uint priority;
        // Type of the message: error | warning | note | hint | docs.
        std::string type;
        // Content of the message.
        Ptr message;

        PointerMessage() {}
        PointerMessage(const json &msg) : priority(MAX_PRIORITY_UINT) {
            ASSUME_HAS(msg, "content");
            message = parse(msg["content"]);

            ASSUME_HAS_STR(msg, "type");
            type = msg["type"];

            if (msg.contains("priority")) {
                ASSUME_HAS_UINT_ASSIGN(msg, priority);
                ASSUME(priority <= MAX_PRIORITY_UINT, "priority exceeds MAX_PRIORITY_UINT");
            }
        }
    };

    struct TemplateData {
        Metadata metadata;
        // Declared macros.
        std::map<std::string, Ptr> macros;

        // -- Message parts --
        Ptr header_message;
        std::map<std::string, PointerMessage> pointer_messages;
        Ptr description;
    
        TemplateData(const dia_file::ParamData &params) {
            cstrr type = params.metadata.type;
            cstrr family = params.metadata.family;
            cstrr name = params.metadata.name;
        
            // Fetch the message template.
            std::string filename = params.metadata.get_path();
            std::ifstream file(filename);
            if (!file.is_open()) {
                // This may not be a bug, but an OS problem on user side,
                // so do not assert. TODO: Exception must be handled.
                throw TemplateFileNotFoundException();
            }

            json template_json = json::parse(file);
        
            // Parse metadata.
            ASSUME_HAS(template_json, "metadata");
            metadata = Metadata(template_json["metadata"]);
            ASSUME(metadata.same_as(params.metadata), "param and template metadata differ");

            // Parse macros.
            const json &macros_json = or_empty(template_json, "macros");
            for (auto &[key, val] : macros_json.items()) {
                macros[key] = parse(val);
            }
            
            // Parse parameters.
            const json &declared_params = or_empty(template_json, "params");
            // Did not provide more than available.
            for (auto &[key, _] : params.params) {
                ASSUME_HAS(declared_params, key);
            }
            // Did not provide fewer than necessary.
            for (auto &[key, val] : declared_params.items()) {
                if (!val.contains("optional") || val["optional"] != true) {
                    ASSUME_HAS(params.params, key);
                }
            }

            // Parse message parts.
            // - header message
            ASSUME_HAS(template_json, "header_message");
            header_message = parse(template_json["header_message"]);
            
            // - pointer messages
            if (template_json.contains("pointer_messages")) {
                ASSUME_OBJ(template_json["pointer_messages"]);
                for (auto &[key, val] : template_json["pointer_messages"].items()) {
                    pointer_messages[key] = PointerMessage(val);
                }
            }

            // - description
            if (template_json.contains("description")) {
                description = parse(template_json["description"]);
            }
        }
    };

} // namespace message_template
} // namespace dia_app
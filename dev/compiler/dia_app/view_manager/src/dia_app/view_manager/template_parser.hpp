#pragma once
#include <fstream>
#include <yaml-cpp/yaml.h>
#include "utils.hpp"
#include "dia_parser.hpp"

namespace dia_app {
namespace message_template {

    struct TemplateElement;
    using Ptr = std::shared_ptr<TemplateElement>;
    Ptr parse(const YAML::Node &msg);

    #define MAX_PRIORITY_UINT 1000000000
    struct PointerMessage {
        // The lower this number, the higher the priority.
        uint priority;
        // Type of the message: error | warning | note | hint | docs.
        std::string type;
        // Content of the message.
        Ptr message;

        PointerMessage() {}
        PointerMessage(const YAML::Node &msg) : priority(MAX_PRIORITY_UINT) {
            // require content
            assert(msg["content"]);
            message = parse(msg["content"]);

            // require type
            assert(msg["type"] && msg["type"].IsScalar());
            type = msg["type"].as<std::string>();

            if (msg["priority"]) {
                priority = msg["priority"].as<unsigned>();
                assert(priority <= MAX_PRIORITY_UINT);
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
                std::cerr << "Failed to open message template!" << std::endl;
                // This may not be a bug, but an OS problem on user side,
                // so do not assert. TODO: Exception must be handled.
                throw TemplateFileNotFoundException();
            }

            YAML::Node template_yaml = YAML::Load(file);
        
            // Parse metadata.
            assert(template_yaml["metadata"]);
            metadata = Metadata(template_yaml["metadata"]);
            assert(metadata.same_as(params.metadata));

            // Parse macros.
            const YAML::Node &macros_node = template_yaml["macros"];
            if (macros_node && macros_node.IsMap()) {
                for (const auto &it : macros_node) {
                    const std::string key = it.first.as<std::string>();
                    macros[key] = parse(it.second);
                }
            }
            
            // Parse parameters.
            const YAML::Node &declared_params = template_yaml["params"];
            if (declared_params && declared_params.IsMap()) {
                // Did not provide more than available.
                for (const auto &it : params.params) {
                    assert(declared_params[it.first]);
                }
                // Did not provide fewer than necessary.
                for (const auto &it : declared_params) {
                    const YAML::Node &val = it.second;
                    if (!val["optional"] || !val["optional"].as<bool>()) {
                        assert(params.params.count(it.first.as<std::string>()));
                    }
                }
            }

            // Parse message parts.
            // - header message
            assert(template_yaml["header_message"]);
            header_message = parse(template_yaml["header_message"]);
            
            // - pointer messages
            if (template_yaml["pointer_messages"]) {
                const YAML::Node &pm_node = template_yaml["pointer_messages"];
                assert(pm_node.IsMap());
                for (const auto &it : pm_node) {
                    const std::string key = it.first.as<std::string>();
                    pointer_messages[key] = PointerMessage(it.second);
                }
            }

            // - description
            if (template_yaml["description"]) {
                description = parse(template_yaml["description"]);
            }
        }
    };

} // namespace message_template
} // namespace dia_app

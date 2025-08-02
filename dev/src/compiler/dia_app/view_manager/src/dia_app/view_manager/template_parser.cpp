#include "template_parser.hpp"

namespace dia_app {
namespace message_template {
    // The maximal value for pointer message priority.
	#define MAX_PRIORITY_UINT 1'000'000'000
	
    PointerMessage::PointerMessage() {}
    PointerMessage::PointerMessage(const YAML::Node &msg) : priority(MAX_PRIORITY_UINT) {
        // require content
        assert(msg["content"]);
        message = parse(msg["content"]);

        // require type
        assert(msg["type"] && msg["type"].IsScalar());
        type = from_string(msg["type"].as<std::string>());

        if (msg["priority"]) {
            priority = msg["priority"].as<unsigned>();
            assert(priority <= MAX_PRIORITY_UINT);
        }
    }

    InfoTemplate::InfoTemplate(const ShortMetadata &short_metadata) {
        // Fetch the message template.
        std::string   filename = short_metadata.getPath();
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open message template: " << filename << std::endl;
            // This may not be a bug, but an OS problem on user side,
            // so do not assert. TODO: Exception must be handled.
            throw TemplateFileNotFoundException();
        }

        YAML::Node template_yaml = YAML::Load(file);

        // Parse metadata.
        assert(template_yaml["metadata"]);
        metadata = Metadata(template_yaml["metadata"]);
        assert(metadata.sameAs(short_metadata));

        // Parse macros.
        const YAML::Node& macros_node = template_yaml["macros"];
        if (macros_node && macros_node.IsMap()) {
            for (const auto& it: macros_node) {
                const std::string key = it.first.as<std::string>();
                macros.put(key, parse(it.second));
            }
        }

        // Parse parameters.
        declared_params = template_yaml["params"];

        // Parse explore edge micro templates.
        const YAML::Node& edge_templates = template_yaml["explore_edges"];
        if (edge_templates && edge_templates.IsMap()) {
            // Parse all edge templates.
            for (const auto& it: edge_templates) {
                const std::string key = it.first.as<std::string>();
                assert(it.second["content"]);
                
                explore_edges.put(key, parse(it.second["content"]));
                declared_explore_edges_params.put(key, it.second["params"]);
            }
        }

        // Parse info sections.
        // - header message
        assert(template_yaml["header_message"]);
        header_message = parse(template_yaml["header_message"]);

        // - pointer messages
        if (template_yaml["pointer_messages"]) {
            const YAML::Node& pm_node = template_yaml["pointer_messages"];
            assert(pm_node.IsMap());
            for (const auto& it: pm_node) {
                const std::string key = it.first.as<std::string>();
                pointer_messages.put(key, PointerMessage(it.second));
            }
        }

        // - description
        if (template_yaml["description"]) description = parse(template_yaml["description"]);
    }

    void InfoTemplate::verify(const dia_file::InfoParams& params) {
        // Verify the global template parameters.
        verify_params(declared_params, params.params);

        // Verify the local explore edge parameters.
        for (auto &[key, val] : explore_edges) {
            // Verify params of all users of this edge template.
            for (const auto& e: params.explore_edges) {
                if (e.name == key) {
                    verify_params(declared_explore_edges_params[key], e.params);
                }
            }
        }
    }

    void InfoTemplate::verify_params(const YAML::Node& declared_params, const base::HashMap<std::string, dia_file::DisplayPtr>& provided_params) {
        if (declared_params && declared_params.IsMap()) {
            // Did not provide more than available.
            for (const auto& it: provided_params) assert(declared_params[it.first]);
            // Did not provide fewer than necessary.
            for (const auto& it: declared_params) {
                const YAML::Node& val = it.second;
                if (!val["optional"] || !val["optional"].as<bool>())
                    assert(provided_params.count(it.first.as<std::string>()));
            }
        }
    }
}
}
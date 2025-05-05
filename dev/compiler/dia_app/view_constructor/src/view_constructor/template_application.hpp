#pragma once
#include "utils.hpp"
#include "template_parser.hpp"
#include "dia_parser.hpp"
#include "template_elements.hpp"

namespace dia_app {
namespace message_template {

    // Apply the message template to message.
    inline dia_file::Ptr apply(Ptr message, TemplateDataHandle handle) {
        if (!message) return nullptr;
        
        // Evaluate the template.
        return message->to_display(handle);
    }

    struct DisplayPointerMessage {
        uint priority;
        std::string type;
        dia_file::Ptr message;

        DisplayPointerMessage() {}
        DisplayPointerMessage(const PointerMessage &msg, TemplateDataHandle handle) :
            priority(msg.priority), type(msg.type) {
            message = apply(msg.message, handle);
            ASSUME(message, "no pointer message after template application");
        }
    };

    struct Info {
        using CodeData = dia_file::CodeData;

        Metadata metadata;
        dia_file::Ptr header_message;
        std::optional<CodeData> code;
        std::map<std::string, DisplayPointerMessage> pointer_messages;
        dia_file::Ptr description;
        std::vector<uint> explore_edges;

        Info(TemplateDataHandle handle) :
            metadata(handle.template_data.metadata),
            code(handle.param_data.code),
            explore_edges(handle.param_data.explore_edges) {

            header_message = apply(handle.template_data.header_message, handle);
            ASSUME(header_message, "no header message after template application");

            for (auto &[key, val] : handle.template_data.pointer_messages) {
                pointer_messages[key] = DisplayPointerMessage(val, handle);
            }

            // Description can be nullptr.
            description = apply(handle.template_data.description, handle);
        }
    };

    enum class Error {
        TemplateFileNotFound
    };

    /*
        Apply a message template identified and parametrized by `data`
        and store the result in `result`.
        
        Return true upon success.
    */
    inline std::expected<Info, Error> apply(
        const dia_file::ParamData &param_data,
        DataHandle data_handle
    ) {
        try {
            TemplateData template_data(param_data);
            return Info(TemplateDataHandle(template_data, param_data, data_handle));
        } catch (TemplateFileNotFoundException e) {
            return std::unexpected(Error::TemplateFileNotFound);
        }
    }

} // namespace message_template
} // namespace dia_app
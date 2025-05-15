#pragma once
#include "code_fragment.hpp"

namespace term_ui {

    struct Message {
        StyleType type;
        std::string id;
        std::string message;
        std::string description;
        std::optional<CodeFragment> code;

        Message(StyleType type, std::string id, const std::string &message, const std::string &description,
                const CodeFragment &code) :
            type(type), id(id), message(message), description(description), code(code) {}
        
        Message(::view::ViewResponse *vm_data) {
            // TODO: make full solution
            // Temporary solution, assumptions:
            // - there's only one diagnostic,
            auto &diag = vm_data->diagnostics(0);
            // - exactly 3 sections (error fragments: header, code, description),
            auto &header_message = diag.sections(0).text_section();
            auto &code_section = diag.sections(1).code_section();
            type = StyleType::Error;
            // - the diagnostic's metadata describe the section,
            // - the metadata does contain both error code and file info (none are missing).
            id = diag.metadata().error_code();
            auto &file = diag.metadata().file_info();

            message = CodePieces(header_message.root()).to_string();
            
            std::map<uint, PointerMessage> ptrs;
            for (uint i = 0; i < diag.hl_messages_size(); ++i) {
                ptrs.emplace(diag.hl_messages(i).tag(), diag.hl_messages(i));
            }
            vec<CodeLine> lines;
            for (uint i = 0; i < code_section.lines_size(); ++i) {
                lines.emplace_back(code_section.lines(i));
            }
            code = CodeFragment(file, lines, ptrs);
        }

        void print() const {
            Style style = get_style(type);

            style.print_name(id);
            style.print_with(": ");

            style.print_main_with(message);
            std::cerr << std::endl;

            if (code.has_value()) {
                code.value().print();
            }

            if (description.size() > 0) {
                std::cerr << std::endl << description << std::endl;
            }
        }
    };

}
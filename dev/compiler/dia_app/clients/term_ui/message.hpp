#pragma once
#include "code_fragment.hpp"

namespace term_ui {

    struct Message {
        StyleType type;
        int id;
        std::string message;
        std::string description;
        CodeFragment code;

        Message(StyleType type, int id, const std::string &message, const std::string &description,
                const CodeFragment &code) :
            type(type), id(id), message(message), description(description), code(code) {}
        
        void print() const {
            Style style = get_style(type);

            style.print_name(id);
            style.print_with(": ");

            style.print_main_with(message);
            std::cout << std::endl;

            code.print();

            if (description.size() > 0) {
                std::cout << std::endl << description << std::endl;
            }
        }
    };

}
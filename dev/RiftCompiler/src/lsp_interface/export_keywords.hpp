#pragma once

#include <string>



namespace lsp_interface {
    class LspInterface {
        public:
            LspInterface();
            std::string get_keyword_list_json();
            std::string get_special_list_json();
            std::string get_operator_list_json();
            std::string get_all_json();
    };
}
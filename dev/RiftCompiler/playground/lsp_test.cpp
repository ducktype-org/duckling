#include <iostream>
#include <lsp_interface/export_keywords.hpp>


int main () {
    lsp_interface::print_keyword_list();
    lsp_interface::print_special_list();
    lsp_interface::print_operator_list();
    lsp_interface::print_all_dict();
}
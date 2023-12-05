#include <iostream>
#include <lsp_interface/export_keywords.hpp>

int main () {
    lsp_interface::LspInterface lspInterface;
    std::cout << lspInterface.get_all_json() << std::endl;
}
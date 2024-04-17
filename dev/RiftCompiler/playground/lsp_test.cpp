#include <iostream>
#include <lsp_interface/export_keywords.hpp>

int main() {
	lsp::LspInterface lspInterface;
	std::cout << lspInterface.getAllJson() << std::endl;
}

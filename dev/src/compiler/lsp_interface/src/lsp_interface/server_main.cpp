#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <lsp/io/standard_io.h>
#include <lsp_interface/compiler_files_management.hpp>
#include <lsp_interface/handlers.hpp>

#include <base/pointers/box.hpp>

#include <init/init.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <iostream>

int main() {
	query::setTrackReverseGraph(true);
	compiler::frontend::use_module_modifier_remove = true;

	init::InitObject _;

	duck_ls::ServerSession session;
	session.setFiles(base::makeBox<duck_ls::CompilerFilesManagement>(base::Ref(&session)));

	lsp::ServerEndpoint endpoint(lsp::io::standardIO());

	duck_ls::registerHandlers(endpoint, session);

	try {
		endpoint.runMessageLoop();
	} catch (const std::exception& e) {
		std::cerr << "duck_ls: " << e.what() << "\n";
		return 1;
	}

	return 0;
}

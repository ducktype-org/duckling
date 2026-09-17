#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <lsp/io/standard_io.h>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include <init/init.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <iostream>

int main() {
	query::setTrackReverseGraph(true);
	compiler::frontend::use_module_modifier_remove = true;

	init::InitObject _;

	lsp::ServerEndpoint endpoint(lsp::io::standardIO());

	duck_ls::ServerSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
	duck_ls::FilesCache    files;
	duck_ls::Compiler      compiler{ base::Ref<duck_ls::ServerSession>(&session),
		                             base::Ref<duck_ls::FilesCache>(&files) };

	session.registerHandlers(compiler);

	try {
		endpoint.runMessageLoop();
	} catch (const std::exception& e) {
		std::cerr << "duck_ls: " << e.what() << "\n";
		return 1;
	}

	return 0;
}

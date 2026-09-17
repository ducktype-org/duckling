#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <lsp/io/socket.h>
#include <lsp/io/standard_io.h>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>
#include <version/version.hpp>

#include <clah/clah.hpp>
#include <clah/clah_class.hpp>
#include <clah/param_builder.hpp>
#include <clah/value_parser.hpp>
#include <init/init.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <iostream>

namespace {

	/**
	 * @brief Serves one client over `stream` until it disconnects.
	 */
	int serve(lsp::io::Stream& stream) {
		lsp::ServerEndpoint endpoint(stream);

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

	/**
	 * @brief Waits on localhost for the one client this server is started for.
	 *
	 * A language server instance belongs to a single client, so the listener is closed as soon
	 * as that client is accepted rather than kept open for more.
	 */
	int serveOverSocket(u16 port) {
		lsp::io::SocketListener listener(port);
		std::cerr << "duck_ls: listening on " << lsp::io::Socket::Localhost << ":"
				  << listener.port() << "\n";

		auto socket = listener.accept();
		listener.close();

		return serve(socket.stream());
	}

	clah::Clah getDuckLsClah() {
		return clah::Clah("duck_ls", "The Duckling language server.")
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('v')
		             .addLongName("version")
		             .addShortDesc("Print version and exit")
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addLongName("version-verbose")
		             .addShortDesc("Print version together with build information and exit")
		             .build())
		    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
		             .addLongName("socket")
		             .addShortDesc("Serve one client on 127.0.0.1:<port> instead of stdio.")
		             .build())
		    .setPreHandler([](const clah::ParsingResult& options) {
				if (options.isFlag("version-verbose")) {
					std::cout << version::renderVerbose("duck_ls") << '\n';
					throw clah::exceptions::SuccessExitException(options);
				}
				if (options.isFlag("version")) {
					std::cout << version::renderShort("duck_ls") << '\n';
					throw clah::exceptions::SuccessExitException(options);
				}
			})
		    .addCustomVerification(
				[](const clah::ParsingResult& options) -> clah::VerificationResult {
					auto port = options.getValue<i64>("socket");
					if (port.empty()) return {};

					if (port.value() < 1 || port.value() > 65'535)
						return std::unexpected<std::string>(
							"--socket must name a port between 1 and 65535"
						);

					return {};
				}
			)
		    .setHandler([](const clah::ParsingResult& options) -> int {
				query::setTrackReverseGraph(true);
				compiler::frontend::use_module_modifier_remove = true;

				auto port = options.getValue<i64>("socket");
				if (port.empty()) return serve(lsp::io::standardIO());

				return serveOverSocket(static_cast<u16>(port.value()));
			});
	}

}

int main(int argc, const char** argv) {
	init::InitObject _;

	auto clah = getDuckLsClah();
	return clah.execute(base::safeIntConv<usize>(argc), argv);
}

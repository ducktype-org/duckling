#pragma once

#include <lsp/server_endpoint.h>
#include <lsp/types.h>
#include <lsp/uri.h>

#include <base/pointers/ref.hpp>

#include <vector>

namespace duck_ls {

	class Compiler;

	/**
	 * @brief The protocol side of the server: it owns no state beyond the endpoint, it only
	 * turns notifications into compiler calls and compiler results into messages.
	 *
	 * The methods are virtual and already implemented; a test subclasses and overrides only the
	 * ones it wants to observe.
	 */
	class ServerSession {
	public:
		explicit ServerSession(base::Ref<lsp::ServerEndpoint> endpoint);

		ServerSession(const ServerSession&)            = delete;
		ServerSession& operator=(const ServerSession&) = delete;
		virtual ~ServerSession()                       = default;

		/**
		 * @brief Sends the diagnostics of one file to the client.
		 */
		virtual void pushDiagnostics(
			const lsp::Uri& uri, const std::vector<lsp::Diagnostic>& diagnostics
		);

		/**
		 * @brief Installs every protocol handler this server implements, each one calling
		 * `compiler`. Both this session and `compiler` must outlive the message loop.
		 */
		virtual void registerHandlers(Compiler& compiler);

	private:
		base::Ref<lsp::ServerEndpoint> endpoint;
		/// The encoding positions are expressed in, as negotiated during initialize.
		lsp::PositionEncodingKind position_encoding = lsp::PositionEncodingKind::UTF16;
	};

}

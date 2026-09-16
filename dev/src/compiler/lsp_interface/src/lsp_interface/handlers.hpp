#pragma once

#include <lsp/server_endpoint.h>
#include <lsp_interface/server_session.hpp>

namespace duck_ls {

	/**
	 * @brief Installs every protocol handler this server implements on `endpoint`.
	 *
	 * The framework owns the lifecycle state machine, so the handlers only supply behaviour.
	 * `session` must outlive `endpoint`.
	 */
	void registerHandlers(lsp::ServerEndpoint& endpoint, ServerSession& session);

}

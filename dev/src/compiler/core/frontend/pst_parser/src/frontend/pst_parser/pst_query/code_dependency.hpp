#pragma once

#include <diagnostic/source_position.hpp>
#include <lexer/token.hpp>
#include <query_framework/context/context.hpp>

namespace pst {
	/**
	 * @brief Returns the minimal set of Source Positions that contain the tokens that are included
	 * in PST dependencies of a given query. Should be used for insight/diagnostics.
	 */
	std::vector<dia::SourcePosition> queryPositionDependencies(query::internal::NodeID);

	template<typename Query>
	std::vector<dia::SourcePosition> queryPositionDependencies(typename Query::QKey key) {
		CORE_UNREACHABLE();
		// query::internal::NodeID node_id = query::internal::makeNodeID<Query>(key);
		// return queryPositionDependencies(node_id);
	}

	/**
	 * @brief Returns the set of tokens that are included in dependencies of a given query.
	 * Should be used for insight/diagnostics, this version is more usefull for things that want to
	 * further analyze like lsp.
	 */
	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::internal::NodeID);

	template<typename Query>
	std::vector<CRef<lexer::Token>> queryTokenDependencies(typename Query::QKey key) {
		CORE_UNREACHABLE();
		// query::internal::NodeID node_id = query::internal::makeNodeID<Query>(key);
		// return queryTokenDependencies(node_id);
	}
}

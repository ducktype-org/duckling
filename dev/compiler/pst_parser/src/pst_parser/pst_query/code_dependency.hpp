#pragma once

#include <diagnostic/source_position.hpp>
#include <query_framework/detail/query_graph/query_graph.hpp>
#include <query_framework/context.hpp>
#include <lexer/token.hpp>

namespace pst {
	/**
	 * @brief Returns the minimal set of Source Positions that contain the tokens that are included in dependencies of a given query.
	 */
	std::vector<dia::SourcePosition> queryPositionDependencies(query::Context&, query::detail::NodeID);

	template<typename Query>
	std::vector<dia::SourcePosition> queryPositionDependencies(query::Context& ctx, typename Query::QKey key) {
		query::detail::NodeID node_id = query::detail::makeNodeID(Query::getID(), key);
		return queryPositionDependencies(ctx, node_id);
	}

	/**
	 * @brief Returns the set of tokens that are included in dependencies of a given query.
	 */
	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::Context&, query::detail::NodeID);

	template<typename Query>
	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::Context& ctx, typename Query::QKey key) {
		query::detail::NodeID node_id = query::detail::makeNodeID(Query::getID(), key);
		return queryTokenDependencies(ctx, node_id);
	}
}

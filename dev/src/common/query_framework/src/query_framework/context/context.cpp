#include "context.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/logger.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>

namespace {
	// Variable to track whether we are inside a query.
	// This is set inside the standardQueryEntry
	// and is used to prevent calling entry points from within queries
	thread_local bool current_thread_inside_query = false;
}

namespace query {
	internal::QueryState Context::main_query_state{};

	bool Context::areWeInsideQuery() { return current_thread_inside_query; }

	bool Context::isAnyQueryCurrentlyRunning() { return main_query_state.activeQueryCount() != 0; }

	void Context::setAreWeInsideQuery(bool value) { current_thread_inside_query = value; }

	void Context::logInt(Box<dia_int::MessageBase> diagnostic) {
		assertActive();
		main_query_state.logDiagnosticForNode(my_node, std::move(diagnostic));
	}

	void Context::collectAllDiagnostic(std::vector<CRef<dia_int::dia_args::Diagnostic>>& output) {
		for (auto& [_, logger]: *main_query_state.getDiagnosticLoggers())
			logger->collectDiagnostics(output);
	}

	Box<dia_int::Logger> Context::dumpToOneLoggerAndClear() {
		Box<dia_int::Logger>          combined_logger = makeBox<dia_int::Logger>();
		std::vector<internal::NodeID> node_ids;
		for (auto& [node, logger]: *main_query_state.getDiagnosticLoggers()) {
			combined_logger->mergeWith(std::move(*logger));
			node_ids.push_back(node);
		}
		for (const auto& node_id: node_ids) main_query_state.clearDiagnosticForNode(node_id);

		return combined_logger;
	}
}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "context.hpp"

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/logger.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>

namespace {
	/**
	 * @brief Tracks whether the current thread is inside a query.
	 * Set inside standardQueryEntry and used to prevent calling entry points from within queries.
	 */
	thread_local bool current_thread_inside_query = false;
}

namespace query {
	internal::QueryState Context::main_query_state{};

	bool Context::areWeInsideQuery() { return current_thread_inside_query; }

	bool Context::isAnyQueryCurrentlyRunning() { return main_query_state.activeQueryCount() != 0; }

	void Context::setAreWeInsideQuery(bool value) { current_thread_inside_query = value; }

	void Context::logInt(Box<dia::MessageBase> diagnostic) {
		assertActive();
		main_query_state.logDiagnosticForNode(my_node, std::move(diagnostic));
	}

	void Context::moveDiagnosticsFrom(dia::Logger& logger) {
		assertActive();
		main_query_state.logDiagnosticFromLoggerForNode(my_node, logger);
	}

	void Context::collectAllDiagnostic(std::vector<CRef<dia::dia_args::Diagnostic>>& output) {
		for (auto& [_, logger]: *main_query_state.getDiagnosticLoggers())
			logger->collectDiagnostics(output);
	}

	void Context::collectAndUpdateAllDiagnostic(
		std::vector<CRef<dia::dia_args::Diagnostic>>& output,
		const dia::UpdatePositionFunc&                update_func
	) {
		for (auto& [_, logger]: *main_query_state.getDiagnosticLoggers())
			logger->collectAndUpdatePositionDiagnostics(output, update_func);
	}

	Box<dia::Logger> Context::dumpToOneLoggerAndClear() {
		Box<dia::Logger>              combined_logger = makeBox<dia::Logger>();
		std::vector<internal::NodeID> node_ids;
		for (auto& [node, logger]: *main_query_state.getDiagnosticLoggers()) {
			combined_logger->logFromLogger(*logger);
			node_ids.push_back(node);
		}
		for (const auto& node_id: node_ids) main_query_state.clearDiagnosticForNode(node_id);

		return combined_logger;
	}
}

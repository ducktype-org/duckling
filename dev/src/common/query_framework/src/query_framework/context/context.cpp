#include "context.hpp"

#include "diagnostic_interactive/core/diagnostic_arguments.hpp"

#include <diagnostic_interactive/logger.hpp>

#include <query_framework/internal/query_graph/query_state.hpp>

namespace query {
	internal::QueryState Context::main_query_state{};

	void Context::logInt(Box<dia_int::MessageBase> diagnostic) {
		assertActive();
		main_query_state.logDiagnosticForNode(my_node, std::move(diagnostic));
	}

	void Context::collectAllDiagnostic(std::vector<CRef<dia_int::dia_args::Diagnostic>>& output) {
		for (auto& [_, logger]: *main_query_state.getDiagnosticLoggers())
			logger->collectDiagnostics(output);
	}
}

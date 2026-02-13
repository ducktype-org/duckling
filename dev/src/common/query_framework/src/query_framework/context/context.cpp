#include "context.hpp"

#include <query_framework/internal/query_graph/query_state.hpp>

namespace query {
	internal::QueryState Context::main_query_state{};

	void Context::logInt(Box<dia_int::MessageBase> diagnostic) {
		assertActive();
		int_logger.log(std::move(diagnostic));
	}
}

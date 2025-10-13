#include "context.hpp"

#include "internal/query_graph/query_state.hpp"

namespace query {
	dia::Logger Context::logger{};

	internal::QueryState Context::main_query_state{};

	void Context::log(Box<dia::Message> message) {
		assertActive();
		logger.log(std::move(message));
	}
}

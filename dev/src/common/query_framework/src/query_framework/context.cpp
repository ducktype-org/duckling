#include "context.hpp"

#include "internal/query_graph/query_state.hpp"

namespace query {
	dia::Logger Context::logger{};

	dia_int::Logger Context::int_logger{};

	internal::QueryState Context::main_query_state{};

	void Context::log(Box<dia::Message> message) {
		assertActive();
		logger.log(std::move(message));
	}

	void Context::logInt(Box<dia_int::MessageBase> diagnostic) {
		assertActive();
		int_logger.log(std::move(diagnostic));
	}
}

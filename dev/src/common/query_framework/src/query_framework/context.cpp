#include "context.hpp"

namespace query {
	dia::Logger Context::logger{};

	detail::QueryGraph Context::main_query_graph{};

	void Context::log(Box<dia::Message> message) {
		assertActive();
		logger.log(std::move(message));
	}
}

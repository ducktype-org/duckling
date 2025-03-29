#include "context.hpp"

namespace query {
	dia::Logger Context::logger{};

	void Context::log(Box<dia::Message> message) {
		assertActive();
		logger.log(std::move(message));
	}

}

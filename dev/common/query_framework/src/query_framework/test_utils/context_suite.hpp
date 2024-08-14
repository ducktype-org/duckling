#pragma once

#include "../query_impl.hpp"

#include <tester/tester.hpp>

#include <functional>

namespace tester {
	class ContextSuite: public TestSuite {
	protected:
		ContextSuite(TestConfig&& config, std::string name): TestSuite(std::move(config), name) {}

		void* withContext(std::function<void*(query::Context&)>);
	};
}

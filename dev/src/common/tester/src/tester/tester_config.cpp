
#include "tester_config.hpp"

namespace tester {

	TestConfig getTestConfig(std::string_view path_to_test_from_dev) {
		TestConfig out;
		out.test_files_path = std::string(CMAKE_SOURCE_DIR) + std::string(path_to_test_from_dev);

		return out;
	}
}

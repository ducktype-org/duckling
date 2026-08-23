#include "log_helpers.hpp"

#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>

#include <diagnostic/placeholder.hpp>

#include <string>

namespace compiler::driver::diagnostics {

	std::function<void(std::string_view, std::string_view, bool)> makeGlobalLoggerReporter() {
		return [](std::string_view header, std::string_view description, bool is_error) {
			if (!global_state::hasGlobalLogger()) return;
			auto& logger = *global_state::getGlobalLogger();
			if (is_error) {
				logger.log(makeBox<dia::PlaceholderError>(
					std::string{ header }, std::string{ description }
				));
			} else {
				logger.log(makeBox<dia::PlaceholderWarning>(
					std::string{ header }, std::string{ description }
				));
			}
		};
	}

}  // namespace compiler::driver::diagnostics

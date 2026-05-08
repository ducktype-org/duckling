#include "log_helpers.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <string>

namespace compiler::driver::diagnostics {

	std::function<void(std::string_view, std::string_view, bool)> makeGlobalLoggerReporter() {
		return [](std::string_view header, std::string_view description, bool is_error) {
			if (!global_state::hasGlobalLogger()) return;
			auto& logger = *global_state::getGlobalLogger();
			if (is_error) {
				logger.log(makeBox<dia_int::PlaceholderError>(
					std::string{ header }, std::string{ description }
				));
			} else {
				logger.log(makeBox<dia_int::PlaceholderWarning>(
					std::string{ header }, std::string{ description }
				));
			}
		};
	}

	void reportMissingPackageInTask(
		base::StrID package_name,
		const std::function<void(std::string_view, std::string_view, bool)>& report
	) {
		report(
			base::strConcat(
				"Package name provided in a compilation task was not found in the provided "
				"package list. Package name: \"",
				package_name.strView(),
				"\""
			),
			std::string{},
			true
		);
	}

}  // namespace compiler::driver::diagnostics

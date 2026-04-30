#include "task.hpp"

#include <driver/manifest/utils.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <json/json.hpp>

namespace compiler::driver {

	namespace ju = compiler::driver::json;

	base::Optional<PackageCompilationTask> PackageCompilationTask::fromJson(
		const nlohmann::json& json
	) {
		ju::checkForUknownFields(
			json,
			{ "package", "strategy" },
			{ "name", "output_file", "linking_options", "archive_options" },
			"task"
		);

		auto package_name = ju::getString(json, "package", "Task requires a package name!");
		if (!package_name) return {};

		auto strategy = ju::getString(json, "strategy", "Task requires a strategy!");
		if (!strategy) return {};

		BuildTarget build_target;

		if (strategy->view() == "dvm") {
			build_target = BuildTargetDVM{};
		} else if (strategy->view() == "native") {
			auto output = ju::getString(json, "output_file", "Native task requires an output file!");
			if (!output) return {};

			base::StrID linking_options;
			if (json.contains("linking_options") && json["linking_options"].is_string()) {
				linking_options = base::StrID(json["linking_options"].get<std::string>());
			}

			build_target = BuildTargetLLVMExecutable{
				.output                     = fs::FilePath(output->str()),
				.additional_linking_options = linking_options,
			};
		} else if (strategy->view() == "lib") {
			auto output = ju::getString(json, "output_file", "Lib task requires an output file!");
			if (!output) return {};

			build_target = BuildTargetLLVMStaticLibrary{
				.output = fs::FilePath(output->str()),
			};
		} else {
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(
					makeBox<dia_int::PlaceholderHeaderError>(
						base::strConcat("Unknown task strategy: \"", strategy->str(), "\""),
						R"(Expected "dvm", "native", or "lib".)"
					)
				);
			}
			return {};
		}

		return PackageCompilationTask{
			.package_name = *package_name,
			.build_target = std::move(build_target),
		};
	}

	base::Optional<Task> Task::fromJson(const nlohmann::json& json) {
		auto task_data = PackageCompilationTask::fromJson(json);
		if (!task_data) return {};

		return Task{
			.type      = TaskType::PackageCompilation,
			.task_data = std::move(*task_data),
		};
	}

}  // namespace compiler::driver

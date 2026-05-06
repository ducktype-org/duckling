#include "task.hpp"

#include <driver/manifest/utils.hpp>

#include "base/collections/optional.hpp"
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>
#include <json/json.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/packages.hpp>

namespace compiler::driver {

	namespace {
		base::Optional<compiler::frontend::ModuleID> getRootModuleIDForRawPackageName(base::StrID package_name) {
			for (const auto& global_package_info : global_state::getPackages()) {
				if (getModuleRef(global_package_info.root_module)->getName() == package_name) {
					return global_package_info.root_module;
				}
			}

			// Get the global logger and log the error if the package is not found
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(
					makeBox<dia_int::PlaceholderError>(
						base::strConcat("Package name provided in a compilation task was not found in the provided package list. Package name: \"", package_name.strView(), "\"")
					)
				);
			}

			return {};
		}
	}

	namespace ju = compiler::driver::json;

	base::Optional<RawPackageCompilationTask> RawPackageCompilationTask::fromJson(
		const nlohmann::json& json
	) {
		if (!ju::checkIsObject(json, "task")) return {};

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
					makeBox<dia_int::PlaceholderError>(
						base::strConcat("Unknown task strategy: \"", strategy->str(), "\""),
						R"(Expected "dvm", "native", or "lib".)"
					)
				);
			}
			return {};
		}

		return RawPackageCompilationTask{
			.package_name = *package_name,
			.build_target = std::move(build_target),
		};
	}

	base::Optional<Task> Task::fromJson(const nlohmann::json& json) {
		auto task_data = RawPackageCompilationTask::fromJson(json);
		if (!task_data) return {};

		return Task{
			.type      = TaskType::PackageCompilation,
			.task_data = std::move(*task_data),
		};
	}

	base::Optional<Task> convertRawTaskToTask(const RawTask& raw_task) {
		variant_match(raw_task.task_data) {
			variant_case(RawPackageCompilationTask, raw_package_task) {
				auto root_module_opt = getRootModuleIDForRawPackageName(raw_package_task.package_name);

				if(!root_module_opt.has_value()) {
					// Error is already logged in getRootModuleIDForRawPackageName, just return empty optional here
					return {};
				}

				return Task{
					.type = TaskType::PackageCompilation,
					.task_data = PackageCompilationTask{
						.root_module = *root_module_opt,
						.build_target = raw_package_task.build_target,
					},
				};
			}
			variant_default {
				CORE_PANIC("Unknown task type in convertRawTaskToTask");
			}
		}
		CORE_UNREACHABLE();
	}

}  // namespace compiler::driver

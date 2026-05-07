#include "task.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <driver/manifest/utils.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <json/json.hpp>

namespace compiler::driver {

	namespace {
		base::Optional<compiler::frontend::ModuleID> getRootModuleIDForRawPackageName(
			base::StrID package_name
		) {
			for (const auto& global_package_info: global_state::getPackages())
				if (getModuleRef(global_package_info.root_module)->getName() == package_name)
					return global_package_info.root_module;

			// Get the global logger and log the error if the package is not found
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"Package name provided in a compilation task was not found in the provided "
						"package list. Package name: \"",
						package_name.strView(),
						"\""
					),
					std::string{}
				));
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
			auto output = ju::getStringIfPresent(
				json, "output_file", "DVM task output_file must be a string"
			);
			build_target = BuildTargetDVM{
				.output_file_stem = output.has_value() ? *output : base::StrID("package_dvm"),
			};
		} else if (strategy->view() == "native") {
			auto output
				= ju::getString(json, "output_file", "Native task requires an output file!");
			if (!output) return {};

			linker::LinkingOptions linking_options{
				.linker_path             = {},
				.additional_link_options = {},
				.link_c_standard_library = true,
			};
			if (json.contains("linking_options")) {
				const auto& linking_json = json["linking_options"];
				if (linking_json.is_string()) {
					auto options_value = ju::getStringValue(
						linking_json, "linking_options", "linking_options must be a string"
					);
					if (!options_value) return {};
					linking_options.additional_link_options = options_value->str();
				} else {
					auto linking_obj = ju::getObjectIfPresent(
						json, "linking_options", "linking_options must be a string or an object"
					);
					if (!linking_obj) return {};
					ju::checkForUknownFields(
						*linking_obj,
						{},
						{ "linker", "additional_link_options", "link_c_standard_library" },
						"linking_options"
					);

					if (linking_obj->contains("linker")) {
						auto linker_path = ju::getStringIfPresent(
							*linking_obj, "linker", "linking_options.linker must be a string"
						);
						if (!linker_path) return {};
						linking_options.linker_path = linker_path->str();
					}

					if (linking_obj->contains("additional_link_options")) {
						auto additional_options = ju::getStringIfPresent(
							*linking_obj,
							"additional_link_options",
							"linking_options.additional_link_options must be a string"
						);
						if (!additional_options) return {};
						linking_options.additional_link_options = additional_options->str();
					}

					if (linking_obj->contains("link_c_standard_library")) {
						auto link_stdlib = ju::getBoolIfPresent(
							*linking_obj,
							"link_c_standard_library",
							"linking_options.link_c_standard_library must be a boolean"
						);
						if (!link_stdlib) return {};
						linking_options.link_c_standard_library = *link_stdlib;
					}
				}
			}

			build_target = BuildTargetLLVMExecutable{
				.output_file_stem = *output,
				.linking_options  = std::move(linking_options),
			};
		} else if (strategy->view() == "lib") {
			auto output = ju::getString(json, "output_file", "Lib task requires an output file!");
			if (!output) return {};

			archiver::ArchivingOptions archiving_options{
				.archiver_path = {},
			};
			if (json.contains("archive_options")) {
				const auto& archive_json = json["archive_options"];
				if (archive_json.is_string()) {
					auto archiver = ju::getStringValue(
						archive_json, "archive_options", "archive_options must be a string"
					);
					if (!archiver) return {};
					archiving_options.archiver_path = archiver->str();
				} else {
					auto archive_obj = ju::getObjectIfPresent(
						json, "archive_options", "archive_options must be a string or an object"
					);
					if (!archive_obj) return {};
					ju::checkForUknownFields(*archive_obj, {}, { "archiver" }, "archive_options");

					if (archive_obj->contains("archiver")) {
						auto archiver_path = ju::getStringIfPresent(
							*archive_obj, "archiver", "archive_options.archiver must be a string"
						);
						if (!archiver_path) return {};
						archiving_options.archiver_path = archiver_path->str();
					}
				}
			}

			build_target = BuildTargetLLVMStaticLibrary{
				.output_file_stem  = *output,
				.archiving_options = std::move(archiving_options),
			};
		} else {
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderError>(
					base::strConcat("Unknown task strategy: \"", strategy->str(), "\""),
					R"(Expected "dvm", "native", or "lib".)"
				));
			}
			return {};
		}

		return RawPackageCompilationTask{
			.package_name = *package_name,
			.build_target = std::move(build_target),
		};
	}

	base::Optional<RawTask> RawTask::fromJson(const nlohmann::json& json) {
		auto task_data = RawPackageCompilationTask::fromJson(json);
		if (!task_data) return {};

		return RawTask{
			.type      = TaskType::PackageCompilation,
			.task_data = std::move(*task_data),
		};
	}

	base::Optional<Task> convertRawTaskToTask(const RawTask& raw_task) {
		variant_match(raw_task.task_data) {
			variant_case(RawPackageCompilationTask, raw_package_task) {
				auto root_module_opt
					= getRootModuleIDForRawPackageName(raw_package_task.package_name);

				if (!root_module_opt.has_value()) {
					// Error is already logged in getRootModuleIDForRawPackageName, just return
					// empty optional here
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
			variant_default { CORE_PANIC("Unknown task type in convertRawTaskToTask"); }
		}
		CORE_UNREACHABLE();
	}

}  // namespace compiler::driver

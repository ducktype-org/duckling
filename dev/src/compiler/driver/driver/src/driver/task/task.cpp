// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "task.hpp"

#include <driver/diagnostics/log_helpers.hpp>
#include <driver_private/standard_library/standard_library.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/str/str_utils.hpp>

#include <json/diagnostics.hpp>
#include <nlohmann/json.hpp>

namespace compiler::driver {

	namespace task {
		namespace {
			void reportMissingPackageInTask(
				base::StrID package_id, const DiagnosticReporter& report
			) {
				report(
					base::strConcat(
						"Package name provided in a compilation task was not found in the provided "
						"package list. Package id: \"",
						package_id.strView(),
						"\""
					),
					std::string{},
					true
				);
			}

			/**
			 * @brief Parses the optional "dvm_linking_options" field of a DVM task.
			 * @param had_error set to true when the field is present but malformed.
			 */
			DVMLinkingOptions parseDVMLinkingOptions(
				const nlohmann::json& json, const DiagnosticReporter& report, bool& had_error
			) {
				DVMLinkingOptions dvm_linking_options{};

				if (!json.contains("dvm_linking_options")) return dvm_linking_options;

				auto options_obj = js::getObjectIfPresent(
					json, "dvm_linking_options", "dvm_linking_options must be an object", report
				);
				if (!options_obj) {
					had_error = true;
					return dvm_linking_options;
				}

				js::checkForUnknownFields(
					*options_obj,
					{},
					{ "shared_libraries", "link_libraries" },
					"dvm_linking_options",
					report
				);

				auto parse_string_array = [&]<typename T>(
											  std::string_view key, std::vector<T>& target
										  ) {
					if (!options_obj->contains(key)) return;

					auto array = js::getArray(
						*options_obj,
						key,
						base::strConcat("dvm_linking_options.", key, " must be an array of strings"),
						report
					);
					if (!array) {
						had_error = true;
						return;
					}

					target.reserve(array->size());
					for (const auto& elem: *array) {
						auto value = js::getStringFromArray(
							elem, base::strConcat("dvm_linking_options.", key), report
						);
						if (value)
							target.emplace_back(value->str());
						else
							had_error = true;
					}
				};

				parse_string_array("shared_libraries", dvm_linking_options.shared_libraries);
				parse_string_array("link_libraries", dvm_linking_options.link_libraries);

				return dvm_linking_options;
			}
		}

		base::Optional<compiler::frontend::ModuleID> getRootModuleIDForRawPackageId(
			base::StrID package_id, const DiagnosticReporter& report
		) {
			for (const auto& global_package_info: global_state::getPackages())
				if (global_package_info.getPackageID() == package_id)
					return global_package_info.getRootModule().illegalAccess().getID();

			reportMissingPackageInTask(package_id, report);
			return {};
		}
	}  // namespace task

	base::Optional<RawPackageCompilationTask> RawPackageCompilationTask::fromJson(
		const nlohmann::json& json, const task::DiagnosticReporter& report
	) {
		if (!js::checkIsObject(json, "task", report)) return {};

		js::checkForUnknownFields(
			json,
			{ "package", "strategy" },
			{ "name", "output_file", "linking_options", "archive_options", "dvm_linking_options" },
			"task",
			report
		);

		bool had_error = false;

		auto package_id = js::getString(json, "package", "Task requires a package id!", report);
		if (!package_id) had_error = true;

		auto strategy = js::getString(json, "strategy", "Task requires a strategy!", report);
		if (!strategy) had_error = true;

		BuildTarget build_target;

		if (strategy && strategy->view() == "dvm_lib") {
			auto output = js::getString(
				json, "output_file", "DVM library task requires an output file!", report
			);
			if (!output) had_error = true;

			build_target = BuildTargetDVMLibrary{
				.output_file_name    = output.copyValueOr(base::StrID("package_dvm.dbc")),
				.dvm_linking_options = task::parseDVMLinkingOptions(json, report, had_error),
			};
		} else if (strategy && strategy->view() == "dvm_exe") {
			auto output = js::getString(
				json, "output_file", "DVM executable task requires an output file!", report
			);
			if (!output) had_error = true;

			build_target = BuildTargetDVMExecutable{
				.output_file_name    = output.copyValueOr(base::StrID("package_dvm.dbc")),
				.dvm_linking_options = task::parseDVMLinkingOptions(json, report, had_error),
			};
		} else if (strategy && strategy->view() == "native") {
			auto output
				= js::getString(json, "output_file", "Native task requires an output file!", report);
			if (!output) had_error = true;

			linker::LinkingOptions linking_options{
				.linker_path             = {},
				.additional_link_options = {},
				.link_c_standard_library = true,
				.stdlib_link_options     = {},
			};
			if (json.contains("linking_options")) {
				const auto& linking_json = json["linking_options"];
				if (linking_json.is_array()) {
					auto options_value
						= js::getArrayOfStrings(linking_json, "linking_options", report);
					if (options_value)
						linking_options.additional_link_options = std::move(*options_value);
					else
						had_error = true;
				} else {
					auto linking_obj = js::getObjectIfPresent(
						json,
						"linking_options",
						"linking_options must be a string or an object",
						report
					);
					if (!linking_obj) {
						had_error = true;
					} else {
						js::checkForUnknownFields(
							*linking_obj,
							{},
							{ "linker", "additional_link_options", "link_c_standard_library" },
							"linking_options",
							report
						);

						if (linking_obj->contains("linker")) {
							auto linker_path = js::getStringIfPresent(
								*linking_obj,
								"linker",
								"linking_options.linker must be a string",
								report
							);
							if (linker_path)
								linking_options.linker_path = linker_path->str();
							else
								had_error = true;
						}

						if (linking_obj->contains("additional_link_options")) {
							auto array = js::getArray(
								*linking_obj,
								"additional_link_options",
								"additional_link_options must be an array of strings",
								report
							);
							if (array) {
								auto additional_options = js::getArrayOfStrings(
									*array, "additional_linking_options", report
								);
								if (additional_options)
									linking_options.additional_link_options
										= std::move(*additional_options);
								else
									had_error = true;
							} else {
								had_error = true;
							}
						}

						if (linking_obj->contains("link_c_standard_library")) {
							auto link_stdlib = js::getBoolIfPresent(
								*linking_obj,
								"link_c_standard_library",
								"linking_options.link_c_standard_library must be a boolean",
								report
							);
							if (link_stdlib)
								linking_options.link_c_standard_library = *link_stdlib;
							else
								had_error = true;
						}
					}
				}
			}

			build_target = BuildTargetLLVMExecutable{
				.output_file_name = output ? *output : base::StrID(),
				.linking_options  = std::move(linking_options),
			};
		} else if (strategy && strategy->view() == "obj") {
			build_target = BuildTargetLLVM{};
		} else if (strategy && strategy->view() == "lib") {
			auto output
				= js::getString(json, "output_file", "Lib task requires an output file!", report);
			if (!output) had_error = true;

			archiver::ArchivingOptions archiving_options{
				.archiver_path = {},
			};
			if (json.contains("archive_options")) {
				const auto& archive_json = json["archive_options"];
				if (archive_json.is_string()) {
					auto archiver = js::getStringValue(
						archive_json, "archive_options", "archive_options must be a string", report
					);
					if (archiver)
						archiving_options.archiver_path = archiver->str();
					else
						had_error = true;
				} else {
					auto archive_obj = js::getObjectIfPresent(
						json,
						"archive_options",
						"archive_options must be a string or an object",
						report
					);
					if (!archive_obj) {
						had_error = true;
					} else {
						js::checkForUnknownFields(
							*archive_obj, {}, { "archiver" }, "archive_options", report
						);

						if (archive_obj->contains("archiver")) {
							auto archiver_path = js::getStringIfPresent(
								*archive_obj,
								"archiver",
								"archive_options.archiver must be a string",
								report
							);
							if (archiver_path)
								archiving_options.archiver_path = archiver_path->str();
							else
								had_error = true;
						}
					}
				}
			}

			build_target = BuildTargetLLVMStaticLibrary{
				.output_file_name  = output ? *output : base::StrID(),
				.archiving_options = std::move(archiving_options),
			};
		} else if (strategy) {
			report(
				base::strConcat("Unknown task strategy: \"", strategy->str(), "\""),
				R"(Expected "dvm_exe", "dvm_lib", "native", "obj", or "lib".)",
				true
			);
			had_error = true;
		}

		if (had_error) return {};

		return RawPackageCompilationTask{
			.package_id   = *package_id,
			.build_target = std::move(build_target),
		};
	}

	base::Optional<RawTask> RawTask::fromJson(
		const nlohmann::json& json, const task::DiagnosticReporter& report
	) {
		auto task_data = RawPackageCompilationTask::fromJson(json, report);
		if (!task_data) return {};

		return RawTask{
			.type      = TaskType::PackageCompilation,
			.task_data = std::move(*task_data),
		};
	}

	/**
	 * @brief Helper function that merges global linking options with task-specific linking options
	 * for a given`BuildTarget`.
	 */
	BuildTarget mergeBuildTargetWithGlobalOptions(
		const BuildTarget& raw_target, const options_types::StdLibOptions& stdlib_options
	) {
		variant_match(raw_target) {
			variant_case(BuildTargetLLVMExecutable, llvm_exec_target) {
				auto linking_options                = llvm_exec_target.linking_options;
				linking_options.stdlib_link_options = getNativeStdLibLinkingArgs(stdlib_options);
				return BuildTargetLLVMExecutable{
					.output_file_name = llvm_exec_target.output_file_name,
					.linking_options  = std::move(linking_options),
				};
			}
			variant_case(BuildTargetDVMExecutable, dvm_exec_target) {
				auto dvm_linking_options = dvm_exec_target.dvm_linking_options;
				base::appendToVector(
					dvm_linking_options.link_libraries,
					getStdLibDVMLinkingDependencies(stdlib_options)
				);
				return BuildTargetDVMExecutable{
					.output_file_name    = dvm_exec_target.output_file_name,
					.dvm_linking_options = std::move(dvm_linking_options),
				};
			}
			variant_default { return raw_target; }
		}
	}

	base::Optional<Task> convertRawTaskToTask(
		const RawTask&                      raw_task,
		const options_types::StdLibOptions& stdlib_options,
		const task::DiagnosticReporter&     report
	) {
		variant_match(raw_task.task_data) {
			variant_case(RawPackageCompilationTask, raw_package_task) {
				auto root_module_opt
					= task::getRootModuleIDForRawPackageId(raw_package_task.package_id, report);

				if (!root_module_opt.has_value()) return {};

				auto build_target = mergeBuildTargetWithGlobalOptions(
					raw_package_task.build_target, stdlib_options
				);

				return Task{
					.type      = TaskType::PackageCompilation,
					.task_data = PackageCompilationTask{
						.root_module = *root_module_opt,
						.build_target = build_target,
					},
				};
			}
			variant_default { CORE_PANIC("Unknown task type in convertRawTaskToTask"); }
		}
		CORE_UNREACHABLE();
	}

	linker::LinkingOptions constructNativeLinkerOptions(
		const options_types::LinkingOptions& linking_options,
		const options_types::StdLibOptions&  stdlib_options
	) {
		return {
			.linker_path = linking_options.native_linker_path,
			.additional_link_options
			= linking_options.native_additional_link_options.copyValueOr(std::vector<std::string>{}),
			.link_c_standard_library = linking_options.native_link_c_standard_lib,
			.stdlib_link_options     = getNativeStdLibLinkingArgs(stdlib_options),
		};
	}

	DVMLinkingOptions constructDVMLinkingOptions(
		const options_types::LinkingOptions& linking_options,
		const options_types::StdLibOptions&  stdlib_options,
		bool                                 is_static_lib
	) {
		auto link_libraries = linking_options.dvm_link_libraries;
		if (not is_static_lib)
			base::appendToVector(link_libraries, getStdLibDVMLinkingDependencies(stdlib_options));

		return { .shared_libraries = linking_options.dvm_shared_libraries,
			     .link_libraries   = std::move(link_libraries) };
	}
}  // namespace compiler::driver

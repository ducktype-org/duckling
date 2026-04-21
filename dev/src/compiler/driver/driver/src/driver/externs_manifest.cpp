#include "externs_manifest.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <global_state/global_logger.hpp>

#include <base/str/str_utils.hpp>

#include <json/json.hpp>

#include <fstream>
#include <unordered_set>

namespace compiler::driver {

	namespace {

		void logManifestError(std::string header, std::string body) {
			if (global_state::hasGlobalLogger()) {
				global_state::getGlobalLogger()->log(
					makeBox<dia_int::PlaceholderHeaderError>(std::move(header), std::move(body))
				);
			}
		}

		base::Optional<options_types::DependencyInfo::CompilationStrategy> parseStrategy(const nlohmann::json& strategy_json, const std::string& dep_name) {
			using Strategy = options_types::DependencyInfo::CompilationStrategy;
			if (!strategy_json.is_object()) {
				logManifestError(
					"Externs manifest: `strategy` must be an object.",
					base::strConcat("Dependency \"", dep_name, "\" has a non-object strategy.")
				);
				return {};
			}
			if (!strategy_json.contains("type") || !strategy_json["type"].is_string()) {
				logManifestError(
					"Externs manifest: `strategy.type` is required and must be a string.",
					base::strConcat(
						"Dependency \"", dep_name, "\" is missing a valid `strategy.type`."
					)
				);
				return {};
			}
			const auto type = strategy_json["type"].get<std::string>();
			if (type == "precompiled")
				return Strategy{ .strategy = Strategy::Precompiled{} };
			if (type == "inline")
				return Strategy{ .strategy = Strategy::InlineCompilation{} };

			logManifestError(
				"Externs manifest: unknown `strategy.type`.",
				base::strConcat(
					"Dependency \"",
					dep_name,
					"\" has `strategy.type` = \"",
					type,
					R"(". Expected "precompiled" or "inline".)"
				)
			);
			return {};
		}

	}  // namespace

	base::Optional<std::vector<options_types::DependencyInfo>> loadExternsManifest(const fs::FilePath& manifest_path) {
		if (!manifest_path.exists()) {
			logManifestError(
				"Externs manifest file not found.",
				base::strConcat("Path: ", manifest_path.string())
			);
			return {};
		}

		std::ifstream input(manifest_path.getPath());
		if (!input.is_open()) {
			logManifestError(
				"Externs manifest file could not be opened for reading.",
				base::strConcat("Path: ", manifest_path.string())
			);
			return {};
		}

		nlohmann::json root;
		try {
			input >> root;
		} catch (const nlohmann::json::parse_error& e) {
			logManifestError(
				"Externs manifest: malformed JSON.",
				base::strConcat("Parser message: ", e.what())
			);
			return {};
		}

		if (!root.is_object() || !root.contains("externs") || !root["externs"].is_array()) {
			logManifestError(
				"Externs manifest: top-level `externs` array is required.",
				"The manifest must be an object with a field `externs` of type array."
			);
			return {};
		}

		std::vector<options_types::DependencyInfo> dependencies;
		std::unordered_set<std::string>            seen_names;

		for (const auto& entry: root["externs"]) {
			if (!entry.is_object()) {
				logManifestError(
					"Externs manifest: each `externs` item must be an object.",
					"An entry in `externs` is not a JSON object."
				);
				return {};
			}
			if (!entry.contains("name") || !entry["name"].is_string()) {
				logManifestError(
					"Externs manifest: extern entry is missing `name`.",
					"Each extern must have a non-empty string `name`."
				);
				return {};
			}
			const auto name = entry["name"].get<std::string>();
			if (name.empty()) {
				logManifestError(
					"Externs manifest: extern `name` must be non-empty.",
					"An extern entry has an empty `name`."
				);
				return {};
			}
			if (!seen_names.insert(name).second) {
				logManifestError(
					"Externs manifest: duplicate dependency name.",
					base::strConcat("Dependency \"", name, "\" appears more than once.")
				);
				return {};
			}
			if (!entry.contains("source") || !entry["source"].is_string()) {
				logManifestError(
					"Externs manifest: extern entry is missing `source`.",
					base::strConcat(
						"Dependency \"", name, "\" must have a string `source` path."
					)
				);
				return {};
			}
			fs::FilePath source_path(entry["source"].get<std::string>());
			if (!source_path.exists()) {
				logManifestError(
					"Externs manifest: extern `source` path does not exist.",
					base::strConcat(
						"Dependency \"", name, "\" has source \"", source_path.string(), "\"."
					)
				);
				return {};
			}
			if (!entry.contains("strategy")) {
				logManifestError(
					"Externs manifest: extern entry is missing `strategy`.",
					base::strConcat(
						"Dependency \"", name, "\" must have a `strategy` object."
					)
				);
				return {};
			}
			auto strategy = parseStrategy(entry["strategy"], name);
			if (!strategy.has_value()) return {};

			dependencies.push_back(options_types::DependencyInfo{
				 .package_info = {
					 .package_name = name,
					 .package_path  = source_path,
				 },
				.compilation_strategy = strategy.value(),
			});
		}

		return dependencies;
	}

}

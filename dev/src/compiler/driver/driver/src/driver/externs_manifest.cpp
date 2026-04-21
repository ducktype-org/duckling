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

		/**
		 * @brief Wraps a JSON object node with a context string for structured field access and
		 * consistent error reporting.
		 */
		class ManifestNode {
		public:
			ManifestNode(const nlohmann::json& json, std::string ctx):
				  json_(json),
				  ctx_(std::move(ctx)) {}

			/** @brief Requires a string field; logs and returns {} if missing or not a string. */
			base::Optional<std::string> requireString(std::string_view key) const {
				const std::string k(key);
				if (!json_.contains(k) || !json_[k].is_string()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` is required and must be a string."
						),
						base::strConcat(ctx_, " must have a valid string `", key, "`.")
					);
					return {};
				}
				return json_[k].get<std::string>();
			}

			/** @brief Like requireString, but also rejects empty values. */
			base::Optional<std::string> requireNonEmptyString(std::string_view key) const {
				auto val = requireString(key);
				if (!val.has_value()) return {};
				if (val->empty()) {
					logManifestError(
						base::strConcat("Packages manifest: `", key, "` must be non-empty."),
						base::strConcat(ctx_, " has an empty `", key, "`.")
					);
					return {};
				}
				return val;
			}

			/** @brief Requires a sub-object field; returns a ManifestNode for it. */
			base::Optional<ManifestNode> requireObject(std::string_view key) const {
				const std::string k(key);
				if (!json_.contains(k) || !json_[k].is_object()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` is required and must be an object."
						),
						base::strConcat(ctx_, " must have a `", key, "` object.")
					);
					return {};
				}
				return ManifestNode(json_[k], base::strConcat(ctx_, " `", key, "`"));
			}

			/** @brief Requires a string field and validates the resulting path exists on disk. */
			base::Optional<fs::FilePath> requireExistingPath(std::string_view key) const {
				auto str = requireString(key);
				if (!str.has_value()) return {};
				fs::FilePath path(*str);
				if (!path.exists()) {
					logManifestError(
						base::strConcat("Packages manifest: `", key, "` path does not exist."),
						base::strConcat(
							ctx_, " has `", key, "` = \"", *str, "\" which does not exist."
						)
					);
					return {};
				}
				return path;
			}

			/**
			 * @brief Requires an array field; returns null on error (missing or wrong type).
			 * The returned pointer is valid as long as this node's underlying JSON is alive.
			 */
			const nlohmann::json* requireArray(std::string_view key) const {
				const std::string k(key);
				if (!json_.contains(k) || !json_[k].is_array()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` is required and must be an array."
						),
						base::strConcat(ctx_, " must have a `", key, "` array.")
					);
					return nullptr;
				}
				return &json_[k];
			}

			/** @brief Optional string field into Optional<string>. Returns false (+ logs) if
			 * present but not a string. */
			bool optionalString(std::string_view key, base::Optional<std::string>& out) const {
				const std::string k(key);
				if (!json_.contains(k)) return true;
				if (!json_[k].is_string()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` must be a string when provided."
						),
						base::strConcat(ctx_, " has invalid `", key, "`.")
					);
					return false;
				}
				out = json_[k].get<std::string>();
				return true;
			}

			/** @brief Optional string field into string. Returns false (+ logs) if present but not
			 * a string. */
			bool optionalString(std::string_view key, std::string& out) const {
				const std::string k(key);
				if (!json_.contains(k)) return true;
				if (!json_[k].is_string()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` must be a string when provided."
						),
						base::strConcat(ctx_, " has invalid `", key, "`.")
					);
					return false;
				}
				out = json_[k].get<std::string>();
				return true;
			}

			/** @brief Optional bool field. Returns false (+ logs) if present but not a boolean. */
			bool optionalBool(std::string_view key, bool& out) const {
				const std::string k(key);
				if (!json_.contains(k)) return true;
				if (!json_[k].is_boolean()) {
					logManifestError(
						base::strConcat(
							"Packages manifest: `", key, "` must be a boolean when provided."
						),
						base::strConcat(ctx_, " has invalid `", key, "`.")
					);
					return false;
				}
				out = json_[k].get<bool>();
				return true;
			}

			const std::string& context() const { return ctx_; }

		private:
			const nlohmann::json& json_;
			std::string           ctx_;
		};

		base::Optional<options_types::DependencyInfo::CompilationStrategy> parseStrategy(
			const ManifestNode& node
		) {
			using Strategy = options_types::DependencyInfo::CompilationStrategy;

			auto type = node.requireString("type");
			if (!type) return {};

			if (*type == "precompiled") return Strategy{ .strategy = Strategy::Precompiled{} };
			if (*type == "inline") return Strategy{ .strategy = Strategy::InlineCompilation{} };

			logManifestError(
				"Packages manifest: unknown `strategy.type`.",
				base::strConcat(
					node.context(),
					" has `type` = \"",
					*type,
					R"(". Expected "precompiled" or "inline".)"
				)
			);
			return {};
		}

		base::Optional<options_types::DependencyInfo> parseDependency(const ManifestNode& node) {
			auto name = node.requireNonEmptyString("name");
			if (!name) return {};
			auto source = node.requireExistingPath("source");
			if (!source) return {};
			auto strat_node = node.requireObject("strategy");
			if (!strat_node) return {};
			auto strategy = parseStrategy(*strat_node);
			if (!strategy) return {};

			return options_types::DependencyInfo{
				.package_info = {
					.package_name = *name,
					.package_path = *source,
					.dependencies = {},
				},
				.compilation_strategy = *strategy,
			};
		}

		base::Optional<BuildTarget> parseBuildTarget(
			const ManifestNode& node, const std::string& output_file_name
		) {
			auto type = node.requireString("type");
			if (!type) return {};

			if (*type == "dvm") return BuildTargetDVM{};

			if (*type == "llvm-static-library") {
				base::Optional<std::string> archiver{};
				if (!node.optionalString("archiver", archiver)) return {};
				return BuildTargetLLVMStaticLibrary{
					.output_file_name  = output_file_name,
					.archiving_options = { .archiver_path = archiver },
				};
			}

			if (*type == "llvm-executable") {
				base::Optional<std::string> linker{};
				std::string                 add_link_opts{};
				bool                        no_cstd = false;
				if (!node.optionalString("linker", linker)
				    || !node.optionalString("additional-link-options", add_link_opts)
				    || !node.optionalBool("no-c-standard-library", no_cstd))
					return {};
				return BuildTargetLLVMExecutable{
					.output_file_name = output_file_name,
					.linking_options  = {
						 .linker_path             = linker,
						 .additional_link_options = add_link_opts,
						 .link_c_standard_library = !no_cstd,
					},
				};
			}

			logManifestError(
				"Packages manifest: unknown `build-target.type`.",
				base::strConcat(
					node.context(),
					" has `type` = \"",
					*type,
					R"(". Expected "dvm", "llvm-executable" or "llvm-static-library".)"
				)
			);
			return {};
		}

	}  // namespace

	base::Optional<std::vector<PackageCompilationManifestEntry>> loadPackagesManifest(
		const fs::FilePath& manifest_path
	) {
		if (!manifest_path.exists()) {
			logManifestError(
				"Packages manifest file not found.",
				base::strConcat("Path: ", manifest_path.string())
			);
			return {};
		}

		std::ifstream input(manifest_path.getPath());
		if (!input.is_open()) {
			logManifestError(
				"Packages manifest file could not be opened for reading.",
				base::strConcat("Path: ", manifest_path.string())
			);
			return {};
		}

		nlohmann::json root;
		try {
			input >> root;
		} catch (const nlohmann::json::parse_error& e) {
			logManifestError(
				"Packages manifest: malformed JSON.", base::strConcat("Parser message: ", e.what())
			);
			return {};
		}

		if (!root.is_object()) {
			logManifestError(
				"Packages manifest: top-level `packages` array is required.",
				"The manifest must be an object with a field `packages` of type array."
			);
			return {};
		}

		ManifestNode          root_node(root, "manifest");
		const nlohmann::json* pkgs = root_node.requireArray("packages");
		if (!pkgs) return {};

		std::vector<PackageCompilationManifestEntry> packages;
		std::unordered_set<std::string>              seen_package_names;

		for (const auto& pkg_entry: *pkgs) {
			if (!pkg_entry.is_object()) {
				logManifestError(
					"Packages manifest: each `packages` item must be an object.",
					"An entry in `packages` is not a JSON object."
				);
				return {};
			}

			// Two-step: first extract name (for context), then build the named node.
			ManifestNode anon_pkg(pkg_entry, "package");
			auto         package_name = anon_pkg.requireNonEmptyString("name");
			if (!package_name) return {};

			if (!seen_package_names.insert(*package_name).second) {
				logManifestError(
					"Packages manifest: duplicate package name.",
					base::strConcat("Package \"", *package_name, "\" appears more than once.")
				);
				return {};
			}

			ManifestNode pkg(pkg_entry, base::strConcat("package \"", *package_name, "\""));

			auto source = pkg.requireExistingPath("source");
			if (!source) return {};
			auto output_file_name = pkg.requireNonEmptyString("output-file-name");
			if (!output_file_name) return {};
			auto build_target_node = pkg.requireObject("build-target");
			if (!build_target_node) return {};
			auto build_target = parseBuildTarget(*build_target_node, *output_file_name);
			if (!build_target) return {};

			const nlohmann::json* deps_array = pkg.requireArray("dependencies");
			if (!deps_array) return {};

			std::vector<options_types::DependencyInfo> dependencies;
			std::unordered_set<std::string>            seen_dep_names;

			for (const auto& dep_entry: *deps_array) {
				if (!dep_entry.is_object()) {
					logManifestError(
						"Packages manifest: each dependency item must be an object.",
						base::strConcat(
							"Package \"", *package_name, "\" contains a non-object dependency entry."
						)
					);
					return {};
				}

				ManifestNode dep_node(
					dep_entry, base::strConcat("dependency in package \"", *package_name, "\"")
				);
				auto dep = parseDependency(dep_node);
				if (!dep) return {};

				if (!seen_dep_names.insert(dep->package_info.package_name).second) {
					logManifestError(
						"Packages manifest: duplicate dependency name within package.",
						base::strConcat(
							"Package \"",
							*package_name,
							"\" contains duplicate dependency \"",
							dep->package_info.package_name,
							"\"."
						)
					);
					return {};
				}

				dependencies.push_back(std::move(*dep));
			}

			packages.push_back(PackageCompilationManifestEntry{
				.package_info = {
					.package_name = *package_name,
					.package_path = *source,
					.dependencies = std::move(dependencies),
				},
				.output_file_name = *output_file_name,
				.build_target     = *build_target,
			});
		}

		return packages;
	}

}

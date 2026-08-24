#include "dbc_linking.hpp"

#include <debug_info/debug_info_io.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/vector_utils.hpp>

#include <artifacts/artifacts.hpp>
#include <logger/logger.hpp>
#include <string_id/string_id.hpp>

#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/loader/loader.hpp>

#include <fstream>
#include <ranges>
#include <string_view>
#include <utility>

namespace compiler::driver {

	void deduplicateCodeCollection(vm::code::CodeCollection& code) {
		base::deduplicateBy(code.functions, [](const vm::code::Function& func) {
			return func.name.str.strView();
		});
		base::deduplicateBy(code.external_c_functions, [](const vm::code::ExternalCFunction& func) {
			return func.name.str.strView();
		});
		base::deduplicateBy(code.global_data, [](const vm::code::GlobalData& g) {
			return g.name.str.strView();
		});
		base::deduplicateBy(code.types, [](const vm::code::TypeOfData& f) {
			return vm::code::typeName(f);
		});
		base::deduplicateBy(code.ffi_functions, [](const vm::code::FFIFunction& func) {
			return func.name.str.strView();
		});
		base::deduplicateBy(code.object_files, [](const std::string& file) { return file; });
	}

	namespace {
		/**
		 * @brief The debug info file of a linked library is assumed to sit next to it, with
		 * the DVM bytecode extension swapped for the debug info one.
		 * @return the path, if such a file exists.
		 */
		base::Optional<fs::FilePath> findLinkLibraryDebugInfoPath(const std::string& link_library_path
		) {
			constexpr std::string_view DBC_EXTENSION = ".dbc";
			constexpr std::string_view DI_EXTENSION  = ".di.json";

			std::string stem = link_library_path;
			if (stem.ends_with(DBC_EXTENSION)) stem.resize(stem.size() - DBC_EXTENSION.size());

			fs::FilePath debug_info_path = stem + std::string(DI_EXTENSION);
			if (!debug_info_path.isRegularFile()) return {};

			return debug_info_path;
		}
	}

	base::OkBad linkDVMPackage(
		const std::vector<artifacts::FileArtifact>& objects,
		const std::vector<artifacts::FileArtifact>& debug_info_artifacts,
		const DVMLinkingOptions&                    dvm_linking_options,
		artifacts::FileArtifact&                    output_file
	) {
		vm::loader::Loader dvm_linker;

		using std::ranges::to;
		using std::ranges::views::transform;

		auto object_files = objects | transform(&artifacts::FileArtifact::file) | to<std::vector>();
		auto link_library_files = dvm_linking_options.link_libraries
		                        | transform([](const std::string& val) { return fs::File(val); })
		                        | to<std::vector>();
		base::appendToVector(object_files, link_library_files);

		auto parse_result = dvm_linker.parseCodeCollectionFromFiles(object_files);

		if (!parse_result.has_value()) {
			CORE_USER_LOG(
				"DVM linking failed: could not parse compiler-generated module bytecode file.\n"
				"Reason: ",
				parse_result.error(),
				"\n"
			);
			return base::BAD;
		}
		vm::code::CodeCollection merged_code = std::move(parse_result.value());

		// This is also a bit hacky here, because we don't have any other place to put this code.
		base::appendToVector(merged_code.object_files, dvm_linking_options.shared_libraries);

		// @TODO: #2895 deal with this once weak/strong symbols are added
		deduplicateCodeCollection(merged_code);


		std::ofstream out(output_file.file.getFilePath().getPath(), std::ios::binary);
		if (!out.is_open()) CORE_PANIC("Failed to open DVM package output file for writing");
		vm::code::serializeCode(merged_code, out);

		std::vector<fs::FilePath> debug_info_paths;
		debug_info_paths.reserve(debug_info_artifacts.size());
		for (auto& art: debug_info_artifacts) debug_info_paths.push_back(art.file.getFilePath());
		for (const auto& link_library: dvm_linking_options.link_libraries)
			if_opt_some(findLinkLibraryDebugInfoPath(link_library), debug_info_path) {
				debug_info_paths.push_back(debug_info_path);
			}

		if (!debug_info_paths.empty()) {
			base::Optional<debug_info::DebugInfo> merged_debug_info;

			for (const auto& debug_info_path: debug_info_paths) {
				std::ifstream in(debug_info_path.getPath(), std::ios::binary);
				if (!in.is_open()) {
					CORE_USER_LOG("DVM: failed to open debug info artifact for merging\n");
					return base::BAD;
				}
				auto di_or_error = debug_info::loadFromStream(in);
				if (!di_or_error.has_value()) {
					CORE_USER_LOG(
						"DVM: failed to parse debug info file: ",
						debug_info_path.string(),
						"\n"
						"Reason: ",
						di_or_error.error(),
						"\n"
					);
					return base::BAD;
				}
				if (!merged_debug_info.has_value())
					merged_debug_info.emplace(std::move(di_or_error.value()));
				else
					merged_debug_info->mergeFrom(std::move(di_or_error.value()));
			}

			if (merged_debug_info.has_value()) {
				merged_debug_info->module_path = output_file.file.getFilePath().string();

				auto output_file_stem = output_file.file.stem();
				// Try to keep the old behaviour, by manually stripping the most common DVM suffix.
				if (output_file_stem.ends_with(".dbc"))
					output_file_stem.resize(output_file_stem.size() - 4);


				auto di_output = output_file.parent->fileArtifactAtOrNew(
					base::StrID(base::strConcat(output_file_stem, ".di.json").c_str())
				);
				std::ofstream di_out(di_output.file.getFilePath().getPath(), std::ios::binary);
				if (!di_out.is_open())
					CORE_PANIC("Failed to open DVM package debug info output file for writing");
				debug_info::saveToStream(*merged_debug_info, di_out);
			}
		}

		return base::OK;
	}
}

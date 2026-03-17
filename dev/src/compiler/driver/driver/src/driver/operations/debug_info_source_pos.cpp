#include "debug_info_source_pos.hpp"

#include "debug_info/debug_info_io.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <global_state/artifacts_location.hpp>

#include <hashing/hash.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <token_source/source.hpp>

#include <fstream>

namespace compiler::driver {

	namespace {
		base::Bit256 hashDebugInfoContent(const artifacts::FileArtifact& artifact) {
			auto content_safe = artifact.file.getContentSafe();
			if (content_safe.has_value())
				return hashing::justHash<hashing::SHA256>(content_safe->view().stringView());

			return hashing::justHash<hashing::SHA256>(artifact.file.getFilePath().string());
		}

		debug_info::FilePosition calculateSourcePosition(const debug_info::PstHashPostion& pos) {
			auto source_position = pst::LangElement::getByStableHash(pos.postion_scope_begin)
			                           .illegalAccess()
			                           .value()
			                           ->getSourcePosition();

			if_opt_some(pos.postion_scope_end, end_hash) {
				auto end_position = pst::LangElement::getByStableHash(end_hash)
				                        .illegalAccess()
				                        .value()
				                        ->getSourcePosition();
				source_position = dia::SourcePosition::merge(source_position, end_position);
			}

			auto [start_line, start_column] = source_position.getStartLineColumn();
			auto [end_line, end_column]     = source_position.getEndLineColumn();

			return debug_info::FilePosition{
				.file_path    = source_position.getSource()->getFile().getFilePath().string(),
				.start_line   = start_line,
				.start_column = start_column,
				.end_line     = end_line,
				.end_column   = end_column,
			};
		}
	}  // namespace

	base::Bit256 KeyOf_DebugInfoResolvePositions::queryUnstablePerfectHash() const {
		return hashDebugInfoContent(input_artifact);
	}

	base::Bit256 KeyOf_DebugInfoResolvePositions::queryStablePerfectHash() const {
		return hashDebugInfoContent(input_artifact);
	}

	struct IMPLEMENT_QUERY(DebugInfoResolvePositions, query::QResult<artifacts::FileArtifact>) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_COPY

		static std::string outputArtifactName(const QKey& key) {
			auto stable_di_name = key.input_artifact.file.name();
			auto base_name      = stable_di_name.substr(0, stable_di_name.size() - DEBUG_INFO_STABLE_EXTENSION.size());
			return base_name.append(DEBUG_INFO_FINAL_EXTENSION);
		}

		static auto provide([[maybe_unused]] query::Context& ctx, const QKey& key) -> PResult {
			std::ifstream input_file(
				key.input_artifact.file.getFilePath().getPath(), std::ios::binary
			);
			if (!input_file.is_open()) {
				CORE_USER_LOG("Failed to open debug-info artifact for reading\n");
				return query::Failed();
			}

			auto debug_info_or_error = debug_info::loadFromStream(input_file);
			if (!debug_info_or_error.has_value()) {
				CORE_USER_LOG(
					"Failed to parse debug-info artifact: ", debug_info_or_error.error(), "\n"
				);
				return query::Failed();
			}

			auto debug_info = std::move(debug_info_or_error.value());
			debug_info.resolvePositions(calculateSourcePosition);

			auto output = getQueryArtifactsCollection()->fileArtifactAtOrNew(
				base::StrID(outputArtifactName(key).c_str())
			);
			debug_info.module_path = output.file.getFilePath().getPath().string();

			std::ofstream output_file(output.file.getFilePath().getPath(), std::ios::binary);
			if (!output_file.is_open()) {
				CORE_USER_LOG("Failed to open resolved debug-info artifact for writing\n");
				return query::Failed();
			}

			debug_info::saveToStream(debug_info, output_file);
			return output;
		}

		static auto loadFromDisc(const QKey& key) -> base::Optional<PResult> {
			auto artifact_name = outputArtifactName(key);
			auto collection    = getQueryArtifactsCollection();

			auto output_maybe = collection->fileArtifactAtMaybe(base::StrID(artifact_name.c_str()));
			if (!output_maybe.has_value()) return {};

			return *output_maybe.value();
		}

		static auto deleteFromDisc(const QKey& key) -> bool {
			auto artifact_name = outputArtifactName(key);
			return getQueryArtifactsCollection()->deleteFileArtifact(
				base::StrID(artifact_name.c_str())
			);
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(DebugInfoResolvePositions);
}

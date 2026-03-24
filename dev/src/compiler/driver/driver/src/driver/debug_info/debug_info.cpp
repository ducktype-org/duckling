#include "debug_info.hpp"

#include <debug_info/debug_info_io.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <global_state/artifacts_location.hpp>

#include <hashing/hash.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/standard_query/query_artifacts_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <token_source/source.hpp>

#include <fstream>

namespace compiler::driver {
	constexpr std::string_view DEBUG_INFO_FINAL_EXTENSION = ".di.json";

	struct ModuleSourceCodeHash {
		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const {
			// 4 random numbers
			return { 62'680'354, 72'959'470, 8'833'575, 82'097'363 };
		}
	};

	/**
	 * @brief This is a placeholder for future input query,
	 * that will track the hash of the source code of the module, so when the source code changes,
	 * the debug info positions will be recalculated.
	 * @TODO: #2329 Change this.
	 *
	 * @note It will always be invalidated, which is for now what we want.
	 */
	DECLARE_QUERY_SIDE_INPUT(SourcePositions, ModuleSourceCodeHash);
	IMPLEMENT_QUERY_SIDE_INPUT(SourcePositions);

	namespace {
		debug_info::FilePosition calculateSourcePosition(
			query::Context& ctx, const debug_info::PstHashPostion& pos
		) {
			auto source_position = pst::LangElement::getByStableHash(pos.postion_scope_begin)
			                           .unlock(ctx)
			                           ->getSourcePosition();

			if_opt_some(pos.postion_scope_end, end_hash) {
				auto end_position
					= pst::LangElement::getByStableHash(end_hash).unlock(ctx)->getSourcePosition();
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

	base::Bit256 KeyOf_DebugInfoForModule::queryStablePerfectHash() const {
		auto component_hash = compiler::frontend::ModuleTree::getPathComponentHash(module_id);
		hashing::addToHash(component_hash.partial, std::to_underlying(backend_type));
		return component_hash.partial.finalize();
	}

	struct IMPLEMENT_QUERY(DebugInfoForModule, query::QResult<artifacts::FileArtifact>) {
		QUERY_ARTIFACTS_MACROS
		QUERY_AUTO_CACHE_COPY

		static std::string outputArtifactName(const QKey& key) {
			return key.queryStablePerfectHash().toStringHex().append(DEBUG_INFO_FINAL_EXTENSION);
		}

		static auto provide([[maybe_unused]] query::Context& ctx, const QKey& key) -> PResult {
			UNPACK_QRESULT_CREF(
				auto compile_module_result =,
				ctx.query<CompileModule>(
					{ key.module_id, key.backend_type, /*build_debug_info=*/true }
				)
			);
			CORE_ASSERT(
				compile_module_result.debug_info.has_value(),
				"CompileModule does not return debug info. This should never happen."
			);

			//  @TODO: #2323 this is an expensive copy, this issue would fix this.
			auto debug_info = compile_module_result.debug_info.value();
			debug_info.resolvePositions(
				[&](const debug_info::PstHashPostion& pos) -> debug_info::FilePosition {
					return calculateSourcePosition(ctx, pos);
				}
			);

			ctx.query<SourcePositions>({});  // We depend on source positions.

			auto output = getQueryArtifactsCollection()->fileArtifactAtOrNew(
				base::StrID(outputArtifactName(key).c_str())
			);

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

	QUERY_IMPLEMENTATION_BOILERPLATE(DebugInfoForModule);
}

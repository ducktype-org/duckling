/**
 * @file incremental_metadata_test_common.hpp
 * @brief Common declarations for metadata persistence tests.
 */
#pragma once

#include <base/types/ints.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/simple_keys.hpp>
#include <string_id/string_id.hpp>

namespace metadata_persistence_test {

	// Simple u64 metadata
	DECLARE_METADATA_SIMPLE(TestCounter, u64);

	// StrID metadata (uses string table optimization)
	DECLARE_METADATA_STRID(TestSourceFile);

}  // namespace metadata_persistence_test

// Query that adds metadata for testing persistence
DECLARE_QUERY(
	MetadataPersistenceTestQuery,
	query::U64Key,
	u64,
	({ .preserve_in_graph = true, .uses_qresult = false })
);

struct IMPLEMENT_QUERY(MetadataPersistenceTestQuery, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		using namespace metadata_persistence_test;

		u64 key_val = key.value;

		// Add simple metadata
		ctx.addMetadataIfNotExists<metadata_TestCounter>(key_val * 10);

		// Add StrID metadata
		ctx.addMetadataIfNotExists<metadata_TestSourceFile>(base::StrID{
			std::string{ "test/source_" } + std::to_string(key_val) + ".duck" });

		return key_val;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(MetadataPersistenceTestQuery);

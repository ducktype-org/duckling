// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <query_framework/internal/query_metadata/metadata_storage.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>

#include <random>

// Those queries are used just to get dummy QueryIDs for NodeID generation.
DECLARE_QUERY(DummyQuery1, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(DummyQuery2, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(DummyQuery3, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DummyQuery1, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery1);

struct IMPLEMENT_QUERY(DummyQuery2, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery2);

struct IMPLEMENT_QUERY(DummyQuery3, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery3);


DECLARE_METADATA(DummyMetadata1, u64);
DECLARE_METADATA(DummyMetadata2, u64);
DECLARE_METADATA(DummyMetadata3, u64);

class MetadataStorageTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MetadataStorageTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(addMetadataTest<1>);
		TESTER_ADD_TEST(addMetadataTest<2>);
		TESTER_ADD_TEST(addMetadataTest<4>);


		TESTER_ADD_TEST(addMetadataIfNotExistTest<1>);
		TESTER_ADD_TEST(addMetadataIfNotExistTest<2>);
		TESTER_ADD_TEST(addMetadataIfNotExistTest<4>);

		TESTER_ADD_TEST(testConcurrentReadsAndWrites<1>);
		TESTER_ADD_TEST(testConcurrentReadsAndWrites<2>);
		TESTER_ADD_TEST(testConcurrentReadsAndWrites<4>);
	}

private:
	template<u64 THREAD_COUNT>
	void addMetadataTest() {
		constexpr u64 OPS_PER_THREAD = 100;
		constexpr u64 HASH_MODULO    = 50;

		query::internal::MetadataStorage storage;

		const auto q_id_1 = DummyQuery1::getID();
		const auto q_id_2 = DummyQuery2::getID();
		const auto q_id_3 = DummyQuery3::getID();

		std::vector<std::jthread> threads;
		threads.reserve(THREAD_COUNT);

		for (u64 i = 0; i < THREAD_COUNT; ++i) {
			threads.emplace_back([&, thread_id = i] {
				std::mt19937_64 rng(thread_id);  // Seed with thread ID for reproducibility
				std::uniform_int_distribution<u64> dist(1, 1'000);

				for (u64 j = 0; j < OPS_PER_THREAD; ++j) {
					auto choose_query = dist(rng) % 3;
					auto node_id      = query::internal::NodeID{
                        (choose_query == 0)   ? q_id_1
							 : (choose_query == 1) ? q_id_2
												   : q_id_3,
						{ dist(rng) % HASH_MODULO }  // Random q hash
					};
					auto metadata_value = dist(rng) % 1'000;
					storage.addMetadata<metadata_DummyMetadata1>(node_id, metadata_value);
				}
			});
		}
		for (auto& thread: threads) thread.join();

		// Verify that metadata was added correctly
		u64 total_metadata_count = 0;
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata1>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata2>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata3>().size();

		ASSERT_EQUAL(total_metadata_count, THREAD_COUNT * OPS_PER_THREAD);

		// Verify that we can retrieve metadata for all nodes
		u64 total_retrieved_count = 0;

		// query 1:
		auto read_metadata = [&]<class MetaDataT, class QueryT>() {
			for (u64 hash = 0; hash < HASH_MODULO; ++hash) {
				auto node_id      = query::internal::NodeID{ QueryT::getID(), { hash } };
				auto metadata_vec = storage.getMetadata<MetaDataT>(node_id);
				total_retrieved_count += metadata_vec.size();

				ASSERT_EQUAL(metadata_vec.size() != 0, storage.hasMetadata<MetaDataT>(node_id));
				ASSERT_EQUAL(metadata_vec.size(), storage.getMetadataCount<MetaDataT>(node_id));

				for (const auto& metadata_value: metadata_vec)
					ASSERT_TRUE(metadata_value->value < 1'000);
			}
		};

		read_metadata.template operator()<metadata_DummyMetadata1, DummyQuery1>();
		read_metadata.template operator()<metadata_DummyMetadata2, DummyQuery1>();
		read_metadata.template operator()<metadata_DummyMetadata3, DummyQuery1>();

		read_metadata.template operator()<metadata_DummyMetadata1, DummyQuery2>();
		read_metadata.template operator()<metadata_DummyMetadata2, DummyQuery2>();
		read_metadata.template operator()<metadata_DummyMetadata3, DummyQuery2>();

		read_metadata.template operator()<metadata_DummyMetadata1, DummyQuery3>();
		read_metadata.template operator()<metadata_DummyMetadata2, DummyQuery3>();
		read_metadata.template operator()<metadata_DummyMetadata3, DummyQuery3>();

		ASSERT_EQUAL_PRINT(total_retrieved_count, total_metadata_count);
	}

	template<u64 THREAD_COUNT>
	void addMetadataIfNotExistTest() {
		constexpr u64 OPS_PER_THREAD = 100;
		constexpr u64 HASH_MODULO    = 50;

		query::internal::MetadataStorage storage;

		const auto q_id_1 = DummyQuery1::getID();
		const auto q_id_2 = DummyQuery2::getID();
		const auto q_id_3 = DummyQuery3::getID();

		// We will add q3 nodes before threads:
		for (u64 hash = 0; hash < HASH_MODULO; ++hash) {
			auto node_id = query::internal::NodeID{ q_id_3, { hash } };
			storage.addMetadata<metadata_DummyMetadata1>(node_id, u64(42));
		}

		std::vector<std::jthread> threads;
		threads.reserve(THREAD_COUNT);

		std::atomic<u64> successful_adds = 0;

		for (u64 i = 0; i < THREAD_COUNT; ++i) {
			threads.emplace_back([&, thread_id = i] {
				std::mt19937_64 rng(thread_id);  // Seed with thread ID for reproducibility
				std::uniform_int_distribution<u64> dist(1, 1'000);

				// choose q1 or q2 or q3:
				for (u64 j = 0; j < OPS_PER_THREAD; ++j) {
					auto choose_query = dist(rng) % 3;
					auto node_id      = query::internal::NodeID{
                        (choose_query == 0)   ? q_id_1
							 : (choose_query == 1) ? q_id_2
												   : q_id_3,
						{ dist(rng) % HASH_MODULO }  // Random q hash
					};

					auto metadata_value = dist(rng) % HASH_MODULO + HASH_MODULO;

					bool added = storage.addMetadataIfNotExists<metadata_DummyMetadata1>(
						node_id, metadata_value
					);
					if (added) successful_adds++;
				}
			});
		}

		for (auto& thread: threads) thread.join();

		// Verify that metadata was added correctly
		u64 total_metadata_count = 0;
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata1>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata2>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata3>().size();
		ASSERT_EQUAL(
			total_metadata_count, successful_adds + HASH_MODULO
		);  // +HASH_MODULO from pre-added q3 nodes

		// verify that no q3 node was modified:
		for (u64 hash = 0; hash < HASH_MODULO; ++hash) {
			auto node_id      = query::internal::NodeID{ q_id_3, { hash } };
			auto metadata_vec = storage.getMetadata<metadata_DummyMetadata1>(node_id);
			ASSERT_EQUAL(metadata_vec.size(), 1);
			ASSERT_EQUAL(metadata_vec[0]->value, 42);
		}
	}

	template<u64 THREAD_COUNT>
	void testConcurrentReadsAndWrites() {
		constexpr u64 OPS_PER_THREAD = 500;
		constexpr u64 HASH_MODULO    = 50;

		query::internal::MetadataStorage storage;

		const auto q_id_1 = DummyQuery1::getID();
		const auto q_id_2 = DummyQuery2::getID();
		const auto q_id_3 = DummyQuery3::getID();

		std::vector<std::jthread> threads;
		threads.reserve(THREAD_COUNT);

		std::atomic<u64> adds = 0;

		for (u64 i = 0; i < THREAD_COUNT; ++i) {
			threads.emplace_back([&, thread_id = i] {
				std::mt19937_64 rng(thread_id);  // Seed with thread ID for reproducibility
				std::uniform_int_distribution<u64> dist(1, 1'000);

				for (u64 j = 0; j < OPS_PER_THREAD; ++j) {
					u64 task = dist(rng) % 4;

					auto choose_query = dist(rng) % 3;
					auto node_id      = query::internal::NodeID{ (choose_query == 0)   ? q_id_1
						                                         : (choose_query == 1) ? q_id_2
						                                                               : q_id_3,
						                                    { dist(rng) % HASH_MODULO } };

					if (task == 0) {
						// add new metadata
						auto metadata_value = dist(rng) % 100;
						storage.addMetadata<metadata_DummyMetadata1>(node_id, metadata_value);
						adds.fetch_add(1);
					} else if (task == 1) {
						// just read
						auto metadata_vec = storage.getMetadata<metadata_DummyMetadata1>(node_id);
						for (const auto& metadata_value: metadata_vec)
							ASSERT_TRUE(metadata_value->value < 100);
					} else if (task == 2) {
						// just check existence
						[[maybe_unused]]
						bool has_metadata
							= storage.hasMetadata<metadata_DummyMetadata1>(node_id);
					} else if (task == 3) {
						// just extract
						auto extracted = storage.extract(node_id);
						if (extracted.has_value()) {
							u64 deleted = 0;
							for (auto& [type_id, metadata_vec]: extracted->type_map)
								deleted += metadata_vec.size();
							adds.fetch_sub(deleted);
						}
					} else {
						CORE_UNREACHABLE();
					}
				}
			});
		}

		for (auto& thread: threads) thread.join();

		// Verify that metadata was added correctly
		u64 total_metadata_count = 0;
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata1>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata2>().size();
		total_metadata_count += storage.getMetadataFromAllNodes<metadata_DummyMetadata3>().size();
		ASSERT_EQUAL_PRINT(total_metadata_count, adds.load());
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");

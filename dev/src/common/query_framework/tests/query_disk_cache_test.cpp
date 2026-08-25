#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <ser/ser.hpp>
#include <tester/tester.hpp>

#include <cstddef>
#include <set>
#include <vector>

// A side input the disk-cached query depends on, so it can be invalidated.
struct KeyOf_DiskSideInput {
	u64 v;

	KeyOf_DiskSideInput(u64 v): v(v) {}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(DiskSideInput, KeyOf_DiskSideInput);

// A stable-hashed query that is cached on disk. It uses an in-memory set as a stand-in for the
// on-disk artifact store so the test does not need the compiler's artifact collection.
struct KeyOf_Disk {
	u64 v;

	KeyOf_Disk(u64 v): v(v) {}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY(
	DiskQuery,
	KeyOf_Disk,
	u64,
	({ .used_hashes             = query::UsedHashes::StableHash,
       .can_be_loaded_from_disk = true,
       .preserve_in_graph       = true,
       .uses_qresult            = false })
);

// A regular (non-disk) query, used to check that its disk-erase hook is a safe no-op.
DECLARE_QUERY(PlainQuery, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DiskQuery, u64) {
	// Stand-in for the on-disk artifact store, keyed by the query's stable key hash.
	static inline std::set<base::Bit256> fake_disk;

	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.query<DiskSideInput>({ key.v });
		fake_disk.insert(key.queryStablePerfectHash());
		return key.v;
	}

	static auto loadFromDisk(const QKey& key) -> base::Optional<PResult> {
		if (fake_disk.contains(key.queryStablePerfectHash())) return PResult{ key.v };
		return {};
	}

	static auto deleteFromDisk(query::QueryStableHash hash) -> bool {
		return fake_disk.erase(hash) > 0;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DiskQuery);

struct IMPLEMENT_QUERY(PlainQuery, u64) {
	static auto provide([[maybe_unused]] Context& ctx, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(PlainQuery);

IMPLEMENT_QUERY_SIDE_INPUT(DiskSideInput);

class DiskCacheCleanup: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DiskCacheCleanup

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		query::setTrackReverseGraph(true);
		TESTER_ADD_TEST(testDiskEraseWiring);
		TESTER_ADD_TEST(testInvalidationDeletesDiskCache);
		TESTER_ADD_TEST(testOrphanDiskCleanup);
	}

private:
	/**
	 * @brief The disk-erase hook is wired for disk-cached queries and panics if wrongly invoked.
	 */
	void testDiskEraseWiring() {
		ImplementationOf_DiskQuery::fake_disk.clear();

		const auto hash = KeyOf_Disk{ 7 }.queryStablePerfectHash();
		ImplementationOf_DiskQuery::fake_disk.insert(hash);

		// DiskQuery's disk-erase hook deletes the on-disk artifact identified by the hash.
		ASSERT_TRUE(DiskQuery::QUERY_DATA.cache_data.disk_erase_function(hash));
		ASSERT_TRUE(not ImplementationOf_DiskQuery::fake_disk.contains(hash));

		// PlainQuery is not cached on disk. Its hook is never reached in real use (callers guard on
		// can_be_loaded_from_disk), so invoking it directly must panic rather than silently no-op.
		ASSERT_TRUE(not PlainQuery::QUERY_DATA.tags.can_be_loaded_from_disk);
		assertThrows<base::Panic>(
			[&] { PlainQuery::QUERY_DATA.cache_data.disk_erase_function(hash); },
			"PlainQuery disk-erase hook should panic when invoked"
		);
	}

	/**
	 * @brief Invalidating a disk-cached query removes its on-disk artifact.
	 */
	void testInvalidationDeletesDiskCache() {
		ImplementationOf_DiskQuery::fake_disk.clear();

		query::entryPoint<DiskQuery>({ 1 });

		const auto disk_hash = KeyOf_Disk{ 1 }.queryStablePerfectHash();
		ASSERT_TRUE(ImplementationOf_DiskQuery::fake_disk.contains(disk_hash));

		// No new inputs => invalidate everything, which removes DiskQuery(1) from the graph.
		query::external::invalidateQueries({});

		ASSERT_TRUE(not ImplementationOf_DiskQuery::fake_disk.contains(disk_hash));
	}

	/**
	 * @brief On-disk artifacts of previous-graph nodes not merged into the current graph are
	 * removed, while live and non-disk nodes are left untouched.
	 */
	void testOrphanDiskCleanup() {
		auto state = query::internal::ContextAccess::getState();
		ImplementationOf_DiskQuery::fake_disk.clear();

		// Orphan: a disk-cached node present only in the previous graph (never demanded now, e.g.
		// because its query hash changed). We never run it, so it stays out of the current graph.
		const auto orphan_node = query::internal::makeNodeID<DiskQuery>(KeyOf_Disk{ 555 });
		const auto orphan_hash = KeyOf_Disk{ 555 }.queryStablePerfectHash();

		// Live: still demanded in the current compilation, so its artifact must be kept.
		query::entryPoint<DiskQuery>({ 1 });
		const auto live_node = query::internal::makeNodeID<DiskQuery>(KeyOf_Disk{ 1 });
		const auto live_hash = KeyOf_Disk{ 1 }.queryStablePerfectHash();

		// A non-disk node in the previous graph must never be touched by the cleanup.
		const auto plain_node = query::internal::makeNodeID<PlainQuery>(query::U64Key{ 9 });

		// Pretend both disk artifacts were written by a previous compilation.
		ImplementationOf_DiskQuery::fake_disk.insert(orphan_hash);
		ImplementationOf_DiskQuery::fake_disk.insert(live_hash);

		// Build a previous graph holding all three nodes and install it. The wire form is
		// plain data, so it goes through `ser` and comes back as itself.
		std::vector<std::byte> prev_bytes;
		ser::write(
			prev_bytes,
			query::internal::QueryGraph::ReducedGraphData{
				.nodes     = { orphan_node, live_node, plain_node },
				.adjacency = { {}, {}, {} },
			}
		)
			.orThrow();
		auto prev_graph = query::internal::QueryGraph::fromReducedGraphData(
			ser::readOrPanicForce<query::internal::QueryGraph::ReducedGraphData>(prev_bytes)
		);
		ASSERT_TRUE(prev_graph.has_value());
		state->setPreviousGraph(std::move(prev_graph).value());

		state->cleanupOrphanedDiskCaches();

		// Only the orphan's artifact is removed.
		ASSERT_TRUE(not ImplementationOf_DiskQuery::fake_disk.contains(orphan_hash));
		ASSERT_TRUE(ImplementationOf_DiskQuery::fake_disk.contains(live_hash));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");

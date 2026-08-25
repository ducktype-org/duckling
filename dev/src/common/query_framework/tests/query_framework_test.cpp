#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/anycast.hpp>
#include <base/types/ints.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>
#include <query_framework/query_errors.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/simple_keys.hpp>
#include <ser/macros.hpp>
#include <ser/ser.hpp>
#include <tester/tester.hpp>

#include <span>
#include <sstream>
#include <type_traits>
#include <vector>

/**
 * @brief The graph on its way to bytes: its wire form is plain data, so `ser::write` is the
 * whole of it.
 */
std::vector<std::byte> writeGraph(const query::internal::QueryGraph& graph) {
	std::vector<std::byte> bytes;
	ser::write(bytes, graph.toReducedGraphData()).orThrow();
	return bytes;
}

/** @brief And back: `ser::read`, then the graph rebuilt from what it read. */
query::internal::QueryGraph readGraph(std::span<const std::byte> bytes) {
	auto reduced = ser::readOrPanicForce<query::internal::QueryGraph::ReducedGraphData>(bytes);
	return query::internal::QueryGraph::fromReducedGraphData(std::move(reduced))
	    .expect("A graph written by writeGraph has to read back");
}

struct Key1 {
	u64            v;
	constexpr auto operator<=>(const Key1& oth) const = default;

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		return v;
	}
};

struct Key2 {
	uint64_t v;

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		return v;
	}
};

DECLARE_QUERY(Fibonacci, Key1, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(FibonacciSum, Key2, u64, ({ .uses_qresult = false }));

/* * * *
 * Q1: *
 * * * */
struct IMPLEMENT_QUERY(Fibonacci, u64) {
	inline static std::map<KHash, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		if (key.v == 0)
			return 0;
		else if (key.v == 1)
			return 1;
		else
			return context.query<Fibonacci>({ key.v - 1 })
			     + context.query<Fibonacci>({ key.v - 2 });
	}

	static auto load(KHash key_hash) -> LoadResult {
		if (cache.contains(key_hash))
			return cache.at(key_hash);
		else
			return {};
	}

	static auto store(KHash key_hash, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key_hash, { .data = res, .acd = acd } });
		return res;
	}

	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash) > 0; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(Fibonacci);


DECLARE_QUERY(FibonacciStringAutoCache, query::U64Key, std::string, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(FibonacciStringAutoCache, std::string) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		return std::to_string(ctx.query<Fibonacci>(Key1{ key.value }));
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibonacciStringAutoCache);

/* * * * *
 * Q2 a: *
 * * * * */
struct IMPLEMENT_QUERY(FibonacciSum, double) {
	static auto provide(Context& context, QKey key) -> PResult {
		double res = 0;
		for (uint64_t i = 0; i <= key.v; i++) res += double(context.query<Fibonacci>(Key1{ i }));
		return res;
	}

	QUERY_AUTO_CACHE_CONSTRUCT
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibonacciSum);


DECLARE_QUERY(CallingEntryPoint, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CallingEntryPoint, u64) {
	static auto provide(Context&, QKey key) -> PResult {
		// call another query without context:
		return query::entryPoint<Fibonacci>({ key.value });
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallingEntryPoint);


DECLARE_QUERY(InsideQueryState, query::U64Key, bool, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(InsideQueryState, bool) {
	static auto provide(Context&, QKey) -> PResult { return Context::areWeInsideQuery(); }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(InsideQueryState);


DECLARE_QUERY(ReferenceQuery, query::U64Key, CRef<u64>, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(ReferenceQuery, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_CREF
};

QUERY_IMPLEMENTATION_BOILERPLATE(ReferenceQuery);


DECLARE_QUERY(
	VectorReferenceQuery, query::U64Key, CRef<std::vector<u64>>, ({ .uses_qresult = false })
);

struct IMPLEMENT_QUERY(VectorReferenceQuery, std::vector<u64>) {
	static auto provide(Context&, QKey key) -> PResult { return { 1, 2, key.value }; }

	QUERY_AUTO_CACHE_CREF
};

QUERY_IMPLEMENTATION_BOILERPLATE(VectorReferenceQuery);

struct Result {
	enum class Status { Live, Destroyed };
	i64                                 id;
	static inline std::map<i64, Status> status;
	static inline i64                   next_id = 0;

	void init() {
		CORE_ASSERT(not status.contains(id), "ID duplication");
		status[id] = Status::Live;
	}

	Result(): id(next_id++) { init(); }

	Result(const Result&): id(next_id++) { init(); }

	Result(Result&& oth) noexcept: id(next_id++) {
		init();
		status.at(oth.id) = Status::Destroyed;
		oth.id            = -1;
	}

	bool validate() {
		if (id == -1) return false;
		return status.at(id) == Status::Live;
	}

	~Result() {
		if (id != -1) status.at(id) = Status::Destroyed;
	}
};

DECLARE_QUERY(LifeTimeQueryStable, query::U64Key, Result, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(LifeTimeQueryStable, Result) {
	static auto provide(Context&, QKey) -> PResult { return {}; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(LifeTimeQueryStable);

DECLARE_QUERY(LifeTimeQueryUnstable, query::U64Key, Result, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(LifeTimeQueryUnstable, Result) {
	static auto provide(Context&, QKey) -> PResult { return {}; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(LifeTimeQueryUnstable);


DECLARE_QUERY(CyclicQuery1, query::U64Key, query::QResult<u64>, ({}));
DECLARE_QUERY(CyclicQuery2, query::U64Key, query::QResult<u64>, ({}));

struct IMPLEMENT_QUERY(CyclicQuery1, query::QResult<u64>) {
	static auto provide(Context& ctx, QKey key) -> PResult { return ctx.query<CyclicQuery2>(key); }

	QUERY_AUTO_CACHE_COPY
};

struct IMPLEMENT_QUERY(CyclicQuery2, query::QResult<u64>) {
	static auto provide(Context& ctx, QKey key) -> PResult { return ctx.query<CyclicQuery1>(key); }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CyclicQuery1);
QUERY_IMPLEMENTATION_BOILERPLATE(CyclicQuery2);

struct ConstructFrom {
	u64 v;
};

struct ConstructTo {
	static inline u64 construct_count = 0;
	u64               v;

	ConstructTo(ConstructFrom from): v(from.v) { construct_count++; }
};

DECLARE_QUERY(ConstructCacheTest, query::U64Key, ConstructTo, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(ConstructCacheTest, ConstructFrom) {
	static auto provide(Context&, QKey key) -> PResult { return { key.value }; }

	QUERY_AUTO_CACHE_CONSTRUCT
};

QUERY_IMPLEMENTATION_BOILERPLATE(ConstructCacheTest);

struct ConstructToViaCRef {
	static inline u64   construct_count = 0;
	CRef<ConstructFrom> v;

	ConstructToViaCRef(CRef<ConstructFrom> from): v(from) { construct_count++; }
};

DECLARE_QUERY(
	ConstructFromCRefCacheTest, query::U64Key, ConstructToViaCRef, ({ .uses_qresult = false })
);

struct IMPLEMENT_QUERY(ConstructFromCRefCacheTest, ConstructFrom) {
	static auto provide(Context&, QKey key) -> PResult { return { key.value }; }

	QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
};

QUERY_IMPLEMENTATION_BOILERPLATE(ConstructFromCRefCacheTest);

namespace context_leak {
	query::Context* leaked_context = nullptr;

	// This is a flag used to check if context leak
	// detection actually took place, and that other
	// assertions did not prevent it
	bool use_leaked_query_happened = false;

	DECLARE_QUERY(IdentityQuery, query::U64Key, u64, ({ .uses_qresult = false }));
	DECLARE_QUERY(LeakQuery, query::U64Key, u64, ({ .uses_qresult = false }));
	DECLARE_QUERY(UseLeakedContext, query::U64Key, u64, ({ .uses_qresult = false }));

	struct IMPLEMENT_QUERY(IdentityQuery, u64) {
		static auto provide(Context&, QKey key) -> PResult { return key.value; }

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(IdentityQuery);

	struct IMPLEMENT_QUERY(LeakQuery, u64) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			leaked_context = &ctx;
			ctx.query<UseLeakedContext>({ 1 });
			return key.value;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LeakQuery);

	struct IMPLEMENT_QUERY(UseLeakedContext, u64) {
		static auto provide(Context&, QKey key) -> PResult {
			use_leaked_query_happened = true;
			leaked_context->query<IdentityQuery>({ 1 });
			return key.value;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(UseLeakedContext);
}

struct KeyOf_SideInput {
	u64 v;

	KeyOf_SideInput(u64 v): v(v) {}

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		CORE_PANIC("Unstable perfect hash should not be used for SideInput");
	}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(SideInput, KeyOf_SideInput);
IMPLEMENT_QUERY_SIDE_INPUT(SideInput);

DECLARE_QUERY(EmptyQuery, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(EmptyQuery, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(EmptyQuery);

DECLARE_QUERY(CallEmptyQueryNTimes, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CallEmptyQueryNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key.value; i++) ctx.query<EmptyQuery>({ i });
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallEmptyQueryNTimes);

DECLARE_QUERY(CallSideInputNTimes, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CallSideInputNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key.value; i++) ctx.query<SideInput>({ i });
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallSideInputNTimes);

using query::utils::withContextCompute;
using query::utils::withContextDo;

struct NoctrKey {
	[[nodiscard]]
	u64 queryUnstablePerfectHash() const noexcept {
		return value;
	}

	static NoctrKey keyCreate(u32 value) noexcept { return NoctrKey{ value }; }

	~NoctrKey() noexcept = default;

	u32 value;


	NoctrKey(NoctrKey&&) noexcept                   = delete;
	NoctrKey(const NoctrKey&) noexcept              = default;
	NoctrKey& operator=(const NoctrKey&) & noexcept = delete;
	NoctrKey& operator=(NoctrKey&&) & noexcept      = delete;

private:
	NoctrKey(u32 value) noexcept: value{ value } {}
};

DECLARE_QUERY(DoesCopyKeys, NoctrKey, u32, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DoesCopyKeys, u32) {
	static auto provide(Context& context, const QKey& key) -> PResult {
		if (key.value == 0)
			return 0;
		else if (key.value == 1)
			return 1;
		else
			return context.query<DoesCopyKeys>(NoctrKey::keyCreate(key.value - 1))
			     + context.query<DoesCopyKeys>(NoctrKey::keyCreate(key.value - 2));
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DoesCopyKeys);

// New: key and query to test stable-vs-unstable perfect hash selection
struct KeyStable final {
	u64                    unstable;
	query::QueryStableHash stable;

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		CORE_PANIC("Should never be called, since StableHashTest uses stable hashes.");
		return unstable;
	}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return stable;
	}
};

DECLARE_QUERY(
	StableHashTest,
	KeyStable,
	u64,
	({
		.used_hashes  = query::UsedHashes::StableHash,
		.uses_qresult = false,
	})
);

struct IMPLEMENT_QUERY(StableHashTest, u64) {
	// record the hash value passed to load()
	static inline query::QueryStableHash last_hash;

	static auto provide(Context&, QKey) -> PResult { return 0; }

	static inline concurrent ::ConHashMap<KHash, query ::CacheEntry<PResult>> cache;

	static auto load(KHash key_hash) -> LoadResult {
		last_hash = key_hash;

		if (const auto& value = cache.atMaybeCopy(key_hash))
			return QResWithACD{ (*value).data, (*value).acd };
		return {};
	}

	static auto store(KHash key_hash, PResult res, query ::ACD acd) -> QResult {
		cache.put(key_hash, { .data = res, .acd = acd });
		return cache.at(key_hash)->data;
	}

	static auto erase([[maybe_unused]] KHash key_hash) -> bool { return false; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(StableHashTest);


using UsesQResult_Result        = query::QResult<u64>;
using UsesQResultNoCatch_Result = query::QResult<u64>;
using NoQResult_Result          = u64;

DECLARE_QUERY(
	UsesQResultTest,
	query::U64Key,
	UsesQResult_Result,
	({
		.uses_qresult                      = true,
		.catch_exceptions_if_using_qresult = true,
	})
);

struct IMPLEMENT_QUERY(UsesQResultTest, UsesQResult_Result) {
	static auto provide(Context&, QKey) -> PResult {
		query::QResult<u64> res = query::Failed();
		return res.valueOrThrow();
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(UsesQResultTest);

DECLARE_QUERY(
	UsesQResultNoCatchTest,
	query::U64Key,
	UsesQResultNoCatch_Result,
	({
		.uses_qresult                      = true,
		.catch_exceptions_if_using_qresult = false,
	})
);

struct IMPLEMENT_QUERY(UsesQResultNoCatchTest, UsesQResultNoCatch_Result) {
	static auto provide(Context&, QKey) -> PResult {
		query::QResult<u64> res = query::Failed();
		return res.valueOrThrow();
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(UsesQResultNoCatchTest);

DECLARE_QUERY(
	NoQResultTest,
	query::U64Key,
	NoQResult_Result,
	({
		.uses_qresult                      = false,
		.catch_exceptions_if_using_qresult = false,
	})
);

struct IMPLEMENT_QUERY(NoQResultTest, NoQResult_Result) {
	static auto provide(Context&, QKey) -> PResult {
		query::QResult<u64> res = query::Failed();
		return res.valueOrThrow();
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(NoQResultTest);

// ========== Metadata Tests Setup ==========

namespace metadata_tests {

	/**
	 * @brief Simple serializable type for testing DECLARE_METADATA macro.
	 * @note SER_DESCRIBE rather than nothing at all: the constructors make this a
	 * non-aggregate, so the automatic member walk cannot reach the fields.
	 */
	struct SerializableData {
		u64         value1 = 0;
		std::string value2;

		SerializableData() = default;

		SerializableData(u64 v1, std::string v2): value1(v1), value2(std::move(v2)) {}

		bool operator==(const SerializableData& other) const {
			return value1 == other.value1 && value2 == other.value2;
		}

		SER_DESCRIBE(value1, value2)
	};

	/**
	 * @brief A simple wrapper around std::string with serialization support.
	 */
	struct StringWrapper {
		std::string value;

		StringWrapper() = default;

		explicit StringWrapper(std::string v): value(std::move(v)) {}

		SER_DESCRIBE(value)
	};

	// Declare metadata types using the macro
	DECLARE_METADATA(TestMeta, SerializableData);

	// SimpleMeta wraps a u64: a scalar needs no more from the macro than a struct does
	DECLARE_METADATA(SimpleMeta, u64);

	// AnotherMeta wraps a string with serialization
	DECLARE_METADATA(AnotherMeta, StringWrapper);

	// StrID metadata for optimized string serialization
	DECLARE_METADATA_STRID(SourceFile);

}  // namespace metadata_tests

// Query with preserve_in_graph = true for testing metadata
DECLARE_QUERY(
	MetadataTestQuery,
	query::U64Key,
	u64,
	({
		.preserve_in_graph = true,
		.uses_qresult      = false,
	})
);

struct IMPLEMENT_QUERY(MetadataTestQuery, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		// Add metadata when key.value > 0
		if (key.value > 0) ctx.addMetadata<metadata_tests::metadata_SimpleMeta>(key.value * 10);
		// Add multiple metadata of same type when key.value > 10
		if (key.value > 10) ctx.addMetadata<metadata_tests::metadata_SimpleMeta>(key.value * 100);
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(MetadataTestQuery);

// Query without preserve_in_graph for testing the assertion
DECLARE_QUERY(
	NonPreservedQuery,
	query::U64Key,
	u64,
	({
		.preserve_in_graph = false,
		.uses_qresult      = false,
	})
);

struct IMPLEMENT_QUERY(NonPreservedQuery, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		// This should panic because preserve_in_graph = false
		ctx.addMetadata<metadata_tests::metadata_SimpleMeta>(key.value);
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(NonPreservedQuery);

// Query that adds multiple types of metadata
DECLARE_QUERY(
	MultiMetadataQuery,
	query::U64Key,
	u64,
	({
		.preserve_in_graph = true,
		.uses_qresult      = false,
	})
);

struct IMPLEMENT_QUERY(MultiMetadataQuery, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.addMetadata<metadata_tests::metadata_SimpleMeta>(key.value);
		ctx.addMetadata<metadata_tests::metadata_AnotherMeta>(metadata_tests::StringWrapper{
			"value_" + std::to_string(key.value) });
		ctx.addMetadata<metadata_tests::metadata_TestMeta>(metadata_tests::SerializableData{
			key.value, "test" });
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(MultiMetadataQuery);

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(autoCacheTest);
		TESTER_ADD_TEST(testConstructCache);
		TESTER_ADD_TEST(testConstructFromCRefCache);
		TESTER_ADD_TEST(testDeps);
		TESTER_ADD_TEST(testSideInput);
		TESTER_ADD_TEST(entryPointSanityTest);
		TESTER_ADD_TEST(resultLifetimeTest<LifeTimeQueryStable>);
		TESTER_ADD_TEST(resultLifetimeTest<LifeTimeQueryUnstable>);
		TESTER_ADD_TEST(withContextDoCompute);
		TESTER_ADD_TEST(queryNamesTest);
		TESTER_ADD_TEST(cycleDetectionTest);
		TESTER_ADD_TEST(debugPrintTest);
		TESTER_ADD_TEST(testContextSanityCheck);
		TESTER_ADD_TEST(serializeDeserializeGraphTest);
		TESTER_ADD_TEST(testQueryResultConcept);
		TESTER_ADD_TEST(testQueryResult);
		TESTER_ADD_TEST(testKeyCopy);
		TESTER_ADD_TEST(stableHashTest);
		TESTER_ADD_TEST(testQueryResultExceptionsHandling);
		// Metadata tests
		TESTER_ADD_TEST(testMetadataBasic);
		TESTER_ADD_TEST(testMetadataMultipleValues);
		TESTER_ADD_TEST(testMetadataMultipleTypes);
		TESTER_ADD_TEST(testMetadataPreserveInGraphCheck);
		TESTER_ADD_TEST(testMetadataSerialization);
	}

private:
	void simpleTest() {
		assertTrue(!query::Context::isAnyQueryCurrentlyRunning(), "Active graph not empty at start");

		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (1)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (2)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 0 }) == 0, "Bad query output (3)");
		assertTrue(query::entryPoint<FibonacciSum>(Key2{ 4 }) == 7, "Bad query output (4)");
		assertTrue(*query::entryPoint<ReferenceQuery>({ 88 }) == 88, "Bad query output (5)");
		assertTrue(
			*query::entryPoint<VectorReferenceQuery>({ 6 }) == std::vector<u64>{ 1, 2, 6 },
			"Bad query output (6)"
		);

		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty after some computations"
		);
	}

	void autoCacheTest() {
		// note: construct cache is tested in testConstructCache

		assertTrue(
			query::entryPoint<FibonacciStringAutoCache>({ 10 }) == "55", "Bad query output (5)"
		);
		assertTrue(
			query::entryPoint<FibonacciStringAutoCache>({ 10 }) == "55", "Bad query output (6)"
		);
	}

	void testDeps() {
#if defined(BUILD_TYPE_DEV)
		const auto& graph = query::Context::getState().getGraph();

		// @TODO: #2138 Change this to proper check, like "doesNodeExist"
		// assertThrows<base::Panic>(
		// 	[&]() { graph.getNodeDeps<EmptyQuery>({ 1 }); }, "Query deps present before query call."
		// );

		query::entryPoint<EmptyQuery>({ 1 });
		auto deps = graph.getNodeDeps<EmptyQuery>({ 1 });
		ASSERT_EQUAL(deps.size(), 1);

		assertThrows<base::Panic>(
			[&]() { graph.getNodeDeps<EmptyQuery>({ 2 }); }, "Query deps present before query call."
		);

		query::entryPoint<CallEmptyQueryNTimes>({ 10 });
		auto deps2 = graph.getNodeDeps<CallEmptyQueryNTimes>({ 10 });
		// 10 + 1 for the query itself:
		ASSERT_EQUAL(deps2.size(), 11);
		{
			auto deps2_filtered
				= graph.getNodeDepsFiltered<CallEmptyQueryNTimes>({ 10 }, EmptyQuery::getID());
			ASSERT_EQUAL(deps2_filtered.size(), 10);
		}
		{
			auto deps2_filtered = graph.getNodeDepsFiltered<CallEmptyQueryNTimes>(
				{ 10 }, CallEmptyQueryNTimes::getID()
			);
			ASSERT_EQUAL(deps2_filtered.size(), 1);
		}
		{
			auto deps2_filtered
				= graph.getNodeDepsFiltered<CallEmptyQueryNTimes>({ 10 }, Fibonacci::getID());
			ASSERT_EQUAL(deps2_filtered.size(), 0);
		}
#endif
	}

	void testSideInput() {
		const auto& graph = query::Context::getState().getGraph();

		// we test that nothing breaks on multiple calls
		for (u64 i = 0; i < 10; i++) {
			query::entryPoint<CallSideInputNTimes>({ 10 });
			auto deps = graph.getNodeDepsFiltered<CallSideInputNTimes>({ 10 }, SideInput::getID());
			ASSERT_EQUAL(deps.size(), 10);
		}
	}

	void entryPointSanityTest() {
#if defined(BUILD_TYPE_DEV)
		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty before some computations"
		);

		assertTrue(
			query::entryPoint<InsideQueryState>({ 1 }),
			"Query execution did not mark the current thread as inside query"
		);

		struct QueryGuard final {
			~QueryGuard() { query::internal::ContextAccess::setAreWeInsideQuery(false); }
		};

		auto context = query::internal::ContextAccess::make(
			query::internal::makeNodeID<CallingEntryPoint>({ 1 })
		);
		query::internal::ContextAccess::setAreWeInsideQuery(true);
		QueryGuard guard;

		assertThrows<base::Panic>(
			[&]() { ImplementationOf_CallingEntryPoint::provide(context, { 1 }); },
			"Calling entry point from query did not panicked."
		);

		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(), "Active graph not empty after some panics"
		);
#endif
	}

	template<class Query>
	void resultLifetimeTest() {
		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty before some computations"
		);

		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res2.validate());
		});

		withContextDo([&](query::Context& ctx) {
			assertTrue(
				query::Context::getState().activeQueryCount() == 1,
				"Active graph should have one node here"
			);

			auto res1 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res2.validate());
		});

		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty after some computations"
		);
	}

	void withContextDoCompute() {
		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Fibonacci>({ 10 });
			ASSERT_TRUE(res1 == 55);

			auto res2 = ctx.query<Fibonacci>({ 10 });
			ASSERT_TRUE(res2 == 55);
		});

		auto res
			= withContextCompute([&](query::Context& ctx) { return ctx.query<Fibonacci>({ 10 }); });
		ASSERT_TRUE(base::anyCast<u64>(res) == 55);
	}

	void queryNamesTest() {
		assertTrue(Fibonacci::QUERY_DATA.name == "Fibonacci", "Bad query name (1)");
		assertTrue(FibonacciSum::QUERY_DATA.name == "FibonacciSum", "Bad query name (2)");
		assertTrue(
			FibonacciStringAutoCache::QUERY_DATA.name == "FibonacciStringAutoCache",
			"Bad query name (3)"
		);
		assertTrue(CallingEntryPoint::QUERY_DATA.name == "CallingEntryPoint", "Bad query name (4)");
		assertTrue(ReferenceQuery::QUERY_DATA.name == "ReferenceQuery", "Bad query name (5)");
		assertTrue(
			VectorReferenceQuery::QUERY_DATA.name == "VectorReferenceQuery", "Bad query name (6)"
		);
		assertTrue(
			LifeTimeQueryStable::QUERY_DATA.name == "LifeTimeQueryStable", "Bad query name (7)"
		);
		assertTrue(
			LifeTimeQueryUnstable::QUERY_DATA.name == "LifeTimeQueryUnstable", "Bad query name (8)"
		);
	}

	void cycleDetectionTest() {
		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty before some computations"
		);

		auto cyclic_call_result = query::entryPoint<CyclicQuery1>({ 1 });
		assertTrue(cyclic_call_result.hasFailed(), "Cyclic query should return a failed QResult");

		assertTrue(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"Active graph not empty after cycle detection"
		);
	}

	void debugPrintTest() {
		const auto& graph = query::Context::getState().getGraph();
		// just check if it compiles and don't throw
		std::stringstream s;
		graph.debugPrintForDrawing(s);
		graph.debugPrint(s);
	}

	void testConstructCache() {
		ConstructTo::construct_count = 0;
		withContextDo([&](query::Context& ctx) {
			// @TODO: #2026 change construct count expectations to 1, 2, 3, after internal_query
			// returns void
			auto res1 = ctx.query<ConstructCacheTest>({ 10 });
			ASSERT_TRUE(res1.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 2);

			auto res2 = ctx.query<ConstructCacheTest>({ 10 });
			ASSERT_TRUE(res2.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 3);

			auto res3 = ctx.query<ConstructCacheTest>({ 20 });
			ASSERT_TRUE(res3.v == 20);
			ASSERT_TRUE(ConstructTo::construct_count == 5);
		});
	}

	void testConstructFromCRefCache() {
		ConstructToViaCRef::construct_count = 0;
		withContextDo([&](query::Context& ctx) {
			// @TODO: #2026 change construct count expectations to 1, 2, 3, after internal_query
			// returns void
			auto res1 = ctx.query<ConstructFromCRefCacheTest>({ 10 });
			ASSERT_TRUE(res1.v->v == 10);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 2);

			auto res2 = ctx.query<ConstructFromCRefCacheTest>({ 10 });
			ASSERT_TRUE(res2.v->v == 10);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 3);

			auto res3 = ctx.query<ConstructFromCRefCacheTest>({ 20 });
			ASSERT_TRUE(res3.v->v == 20);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 5);
		});
	}

	void testContextSanityCheck() {
		// @TODO: #2138 This tests will be hard to bring back, but maybe we can explicitly test
		// contexts active flags here.
		return;

#if defined(BUILD_TYPE_DEV)
		assertThrows<base::Panic>(
			[&]() { query::entryPoint<context_leak::LeakQuery>({ 1 }); },
			"Bad context usage not detected"
		);
		assertTrue(
			context_leak::use_leaked_query_happened,
			"Something else happened, the test is inconclusive"
		);
#endif
	}

	void serializeDeserializeGraphTest() {
		// Create a query graph by making some query calls
		query::entryPoint<Fibonacci>(Key1{ 10 });

		const auto& graph = query::Context::getState().getGraph();
		// Serialize the graph - the wire form is plain data, so `ser` needs nothing else
		auto serialized_data = writeGraph(graph);

		auto deserialized_graph = readGraph(serialized_data);

		auto serialized_data2 = writeGraph(deserialized_graph);

		auto deserialized_graph2 = readGraph(serialized_data2);

		ASSERT_EQUAL(serialized_data.size(), serialized_data2.size());

		ASSERT_TRUE(graph.compare(deserialized_graph));
		ASSERT_TRUE(deserialized_graph2.compare(graph));

		query::entryPoint<Fibonacci>(Key1{ 30 });

		const auto& graph2 = query::Context::getState().getGraph();

		auto serialized_data3    = writeGraph(graph2);
		auto deserialized_graph3 = readGraph(serialized_data3);

		ASSERT_TRUE(serialized_data3.size() != serialized_data2.size());
		ASSERT_TRUE(graph2.compare(deserialized_graph3));
		ASSERT_TRUE(!deserialized_graph3.compare(deserialized_graph2));
	}

	void testQueryResultConcept() {
		static_assert(query::IsQResult<query::QResult<u64>>::value, "QResult concept failed (1)");
		static_assert(!query::IsQResult<u64>::value, "QResult concept failed (2)");

		// not a q result:
		struct QResult {};

		static_assert(!query::IsQResult<QResult>::value, "QResult concept failed (3)");
	}

	void testQueryResult() {
		using namespace query;

		query::QResult<int> hr1 = 1;
		ASSERT_HAS_VALUE(hr1);
		ASSERT_EQUAL(1, hr1.valueOrPanic());

		int                      temp_val = hr1.valueOrPanic();
		base::Optional<Ref<int>> opt1     = Ref<int>(&temp_val);
		ASSERT_HAS_VALUE(opt1);
		ASSERT_EQUAL(1, **opt1);

		query::QResult<std::string> hr2 = "Value";
		// base::Optional<std::string>        stolen_opt = std::move(hr2).optValueMove();
		// ASSERT_EQUAL("Value", stolen_opt);

		std::string                      info  = "Hello";
		query::QResult<std::string_view> whoa2 = std::string_view(info);
		ASSERT_HAS_VALUE(whoa2);
	}

	void testKeyCopy() {
		assertEqual(
			query::entryPoint<DoesCopyKeys>(NoctrKey::keyCreate(4)), 3, "Should be fibonacci(4) = 3"
		);
	}

	void stableHashTest() {
		KeyStable key{ .unstable = 0x12'34u,
			           .stable   = query::QueryStableHash{ 0x11'11u, 0x22'22u } };

		query::entryPoint<StableHashTest>(key);

		query::perfectHashKey<true>(key);
		std::cout << ImplementationOf_StableHashTest::QueryType::QUERY_DATA.usesStableHashing()
				  << "\n";
		std::cout << "Last hash: " << ImplementationOf_StableHashTest::last_hash << "\n";
		std::cout << "Expected : " << key.stable << "\n";
		std::cout << "Unexpected: " << key.unstable << "\n";

		ASSERT_TRUE(ImplementationOf_StableHashTest::last_hash == key.stable);
	}

	void testQueryResultExceptionsHandling() {
		auto result = query::entryPoint<UsesQResultTest>({ 1 });
		assertTrue(result.hasFailed(), "Expected error in UsesQResultTest");

		// @TODO: #2138 decide what to do with commented parts of this test, likely remove them, as
		// they test inner query entry panics.

		// assertThrows<base::Panic>(
		// 	[] { query::entryPoint<UsesQResultNoCatchTest>({ 1 }); },
		// 	"QueryFailedException not thrown as expected"
		// );

		// assertThrows<base::Panic>(
		// 	[] { query::entryPoint<NoQResultTest>({ 1 }); },
		// 	"QueryFailedException not thrown as expected"
		// );

		assertThrows<query::internal::QueryFailedException>(
			[] {
				query::QResult<u64> res = query::Failed();
				res.valueOrThrow();
			},
			"QueryFailedException not thrown as expected"
		);

		assertThrows<base::Panic>(
			[] {
				query::QResult<u64> res = query::Failed();
				res.valueOrPanic();
			},
			"Panic not thrown as expected"
		);
	}

	// ========== Metadata Tests ==========

	void testMetadataBasic() {
		using namespace metadata_tests;

		// Call query that adds metadata
		auto result = query::entryPoint<MetadataTestQuery>({ 5 });
		ASSERT_EQUAL(result, 5);

		// Retrieve the metadata
		const auto& state   = query::Context::getState();
		auto        node_id = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 5 });

		auto metadata_vec = state.getMetadata<metadata_SimpleMeta>(node_id);
		ASSERT_EQUAL(metadata_vec.size(), 1);
		ASSERT_EQUAL(metadata_vec[0]->value, 50);  // key.value * 10 = 5 * 10 = 50
	}

	void testMetadataMultipleValues() {
		using namespace metadata_tests;

		// Call query that adds multiple metadata of same type (key > 10)
		auto result = query::entryPoint<MetadataTestQuery>({ 15 });
		ASSERT_EQUAL(result, 15);

		const auto& state   = query::Context::getState();
		auto        node_id = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 15 });

		auto metadata_vec = state.getMetadata<metadata_SimpleMeta>(node_id);
		ASSERT_EQUAL(metadata_vec.size(), 2);

		// Check values: 15 * 10 = 150 and 15 * 100 = 1500
		bool found_150  = false;
		bool found_1500 = false;
		for (const auto& meta: metadata_vec) {
			if (meta->value == 150) found_150 = true;
			if (meta->value == 1'500) found_1500 = true;
		}
		ASSERT_TRUE(found_150);
		ASSERT_TRUE(found_1500);
	}

	void testMetadataMultipleTypes() {
		using namespace metadata_tests;

		// Call query that adds multiple types of metadata
		auto result = query::entryPoint<MultiMetadataQuery>({ 42 });
		ASSERT_EQUAL(result, 42);

		const auto& state   = query::Context::getState();
		auto        node_id = query::internal::makeNodeID<MultiMetadataQuery>(query::U64Key{ 42 });

		// Check SimpleMeta
		auto simple_vec = state.getMetadata<metadata_SimpleMeta>(node_id);
		ASSERT_EQUAL(simple_vec.size(), 1);
		ASSERT_EQUAL(simple_vec[0]->value, 42);

		// Check AnotherMeta
		auto another_vec = state.getMetadata<metadata_AnotherMeta>(node_id);
		ASSERT_EQUAL(another_vec.size(), 1);
		ASSERT_EQUAL(another_vec[0]->value.value, "value_42");

		// Check TestMeta (with SerializableData)
		auto test_vec = state.getMetadata<metadata_TestMeta>(node_id);
		ASSERT_EQUAL(test_vec.size(), 1);
		ASSERT_EQUAL(test_vec[0]->value.value1, 42);
		ASSERT_EQUAL(test_vec[0]->value.value2, "test");

		// Check that hasMetadata works
		ASSERT_TRUE(state.hasMetadata<metadata_SimpleMeta>(node_id));
		ASSERT_TRUE(state.hasMetadata<metadata_AnotherMeta>(node_id));
		ASSERT_TRUE(state.hasMetadata<metadata_TestMeta>(node_id));

		// Check getMetadataCount
		ASSERT_EQUAL(state.getMetadataCount<metadata_SimpleMeta>(node_id), 1);
		ASSERT_EQUAL(state.getMetadataCount<metadata_AnotherMeta>(node_id), 1);
		ASSERT_EQUAL(state.getMetadataCount<metadata_TestMeta>(node_id), 1);
	}

	void testMetadataPreserveInGraphCheck() {
		// @TODO: #2138 Figure out if we can re-enable this test in some form.
		return;

#if defined(BUILD_TYPE_DEV)
		// Calling NonPreservedQuery should panic because it tries to add metadata
		// to a query without preserve_in_graph = true
		assertThrows<base::Panic>(
			[] { query::entryPoint<NonPreservedQuery>({ 1 }); },
			"Adding metadata to non-preserved query should panic"
		);
#endif
	}

	void testMetadataSerialization() {
		using namespace metadata_tests;

		// Test SerializableData serialization/deserialization
		SerializableData       original{ 123, "hello world" };
		std::vector<std::byte> serialized;
		ASSERT_TRUE(ser::write(serialized, original).hasValue());
		auto deserialized = ser::read<SerializableData>(serialized);
		ASSERT_TRUE(deserialized.hasValue());

		ASSERT_EQUAL(original.value1, deserialized->value.value1);
		ASSERT_EQUAL(original.value2, deserialized->value.value2);

		// Test metadata_TestMeta, which writes and reads through the metadata archive pair
		metadata_TestMeta      meta{ original };
		std::vector<std::byte> meta_serialized;
		{
			query::internal::MetadataOut out{ meta_serialized };
			ASSERT_TRUE(meta.serWrite(out) == ser::Errc::Ok);
			ASSERT_TRUE(out.finish() == ser::Errc::Ok);
		}
		query::internal::MetadataIn in{ meta_serialized };
		const metadata_TestMeta     meta_deserialized = metadata_TestMeta::serMake(in);

		ASSERT_EQUAL(meta.value.value1, meta_deserialized.value.value1);
		ASSERT_EQUAL(meta.value.value2, meta_deserialized.value.value2);

		// Test metadata storage directly
		query::internal::MetadataStorage storage;
		auto test_node = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 99'999 });

		// Initially empty
		auto empty_vec = storage.getMetadata<metadata_SimpleMeta>(test_node);
		ASSERT_EQUAL(empty_vec.size(), 0);
		ASSERT_TRUE(!storage.hasMetadata<metadata_SimpleMeta>(test_node));

		// Add some metadata
		storage.addMetadata<metadata_SimpleMeta>(test_node, u64{ 100 });
		storage.addMetadata<metadata_SimpleMeta>(test_node, u64{ 200 });

		auto vec = storage.getMetadata<metadata_SimpleMeta>(test_node);
		ASSERT_EQUAL(vec.size(), 2);
		ASSERT_TRUE(storage.hasMetadata<metadata_SimpleMeta>(test_node));
		ASSERT_EQUAL(storage.getMetadataCount<metadata_SimpleMeta>(test_node), 2);

		// Clear and verify
		storage.clearNodeMetadata(test_node);
		auto cleared_vec = storage.getMetadata<metadata_SimpleMeta>(test_node);
		ASSERT_EQUAL(cleared_vec.size(), 0);

		// =========================================================
		// Test full MetadataStorage serialize/deserialize roundtrip
		// =========================================================
		query::internal::MetadataStorage storage2;

		auto node1 = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 1 });
		auto node2 = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 2 });
		auto node3 = query::internal::makeNodeID<MetadataTestQuery>(query::U64Key{ 3 });

		// Add various metadata types to different nodes
		storage2.addMetadata<metadata_SimpleMeta>(node1, u64{ 111 });
		storage2.addMetadata<metadata_SimpleMeta>(node1, u64{ 222 });
		storage2.addMetadata<metadata_SimpleMeta>(node2, u64{ 333 });

		storage2.addMetadata<metadata_TestMeta>(node1, SerializableData{ 42, "test string" });
		storage2.addMetadata<metadata_TestMeta>(node3, SerializableData{ 99, "another string" });

		storage2.addMetadata<metadata_AnotherMeta>(node2, StringWrapper{ "wrapped text" });

		// Add StrID metadata (optimized string serialization)
		storage2.addMetadata<metadata_SourceFile>(
			node1, base::StrID{ std::string{ "src/main.duck" } }
		);
		storage2.addMetadata<metadata_SourceFile>(
			node2, base::StrID{ std::string{ "src/main.duck" } }
		);  // Same string - will be deduplicated
		storage2.addMetadata<metadata_SourceFile>(
			node3, base::StrID{ std::string{ "src/parser.duck" } }
		);

		// Serialize the entire storage
		std::vector<std::byte> serialized_storage;
		ASSERT_TRUE(ser::write(serialized_storage, storage2).hasValue());
		ASSERT_TRUE(serialized_storage.size() > 0);

		// Deserialize into a new storage
		auto restored_result = ser::read<query::internal::MetadataStorage>(serialized_storage);
		ASSERT_TRUE(restored_result.hasValue());
		auto restored = std::move(*restored_result).take();

		// Verify SimpleMeta on node1
		auto restored_simple1 = restored.getMetadata<metadata_SimpleMeta>(node1);
		ASSERT_EQUAL(restored_simple1.size(), 2);
		ASSERT_EQUAL(restored_simple1[0]->value, u64{ 111 });
		ASSERT_EQUAL(restored_simple1[1]->value, u64{ 222 });

		// Verify SimpleMeta on node2
		auto restored_simple2 = restored.getMetadata<metadata_SimpleMeta>(node2);
		ASSERT_EQUAL(restored_simple2.size(), 1);
		ASSERT_EQUAL(restored_simple2[0]->value, u64{ 333 });

		// Verify TestMeta on node1
		auto restored_test1 = restored.getMetadata<metadata_TestMeta>(node1);
		ASSERT_EQUAL(restored_test1.size(), 1);
		ASSERT_EQUAL(restored_test1[0]->value.value1, 42);
		ASSERT_EQUAL(restored_test1[0]->value.value2, std::string{ "test string" });

		// Verify TestMeta on node3
		auto restored_test3 = restored.getMetadata<metadata_TestMeta>(node3);
		ASSERT_EQUAL(restored_test3.size(), 1);
		ASSERT_EQUAL(restored_test3[0]->value.value1, 99);
		ASSERT_EQUAL(restored_test3[0]->value.value2, std::string{ "another string" });

		// Verify AnotherMeta on node2
		auto restored_another2 = restored.getMetadata<metadata_AnotherMeta>(node2);
		ASSERT_EQUAL(restored_another2.size(), 1);
		ASSERT_EQUAL(restored_another2[0]->value.value, std::string{ "wrapped text" });

		// Verify StrID metadata (SourceFile)
		auto restored_source1 = restored.getMetadata<metadata_SourceFile>(node1);
		ASSERT_EQUAL(restored_source1.size(), 1);
		ASSERT_EQUAL(restored_source1[0]->value.strView(), std::string_view{ "src/main.duck" });

		auto restored_source2 = restored.getMetadata<metadata_SourceFile>(node2);
		ASSERT_EQUAL(restored_source2.size(), 1);
		ASSERT_EQUAL(restored_source2[0]->value.strView(), std::string_view{ "src/main.duck" });

		auto restored_source3 = restored.getMetadata<metadata_SourceFile>(node3);
		ASSERT_EQUAL(restored_source3.size(), 1);
		ASSERT_EQUAL(restored_source3[0]->value.strView(), std::string_view{ "src/parser.duck" });

		// Verify nodes that should have no metadata of certain types
		ASSERT_TRUE(!restored.hasMetadata<metadata_SimpleMeta>(node3));
		ASSERT_TRUE(!restored.hasMetadata<metadata_TestMeta>(node2));
		ASSERT_TRUE(!restored.hasMetadata<metadata_AnotherMeta>(node1));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");

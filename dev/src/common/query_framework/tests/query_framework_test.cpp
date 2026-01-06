#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/anycast.hpp>
#include <base/types/ints.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_errors.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_input.hpp>
#include <query_framework/query_input_impl.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/simple_keys.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <sstream>
#include <type_traits>

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

	static auto load([[maybe_unused]] KHash key_hash) -> LoadResult { return {}; }

	static auto store([[maybe_unused]] KHash key_hash, PResult res, [[maybe_unused]] query::ACD acd)
		-> QResult {
		return QResult(res);
	}
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


DECLARE_QUERY(CyclicQuery1, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(CyclicQuery2, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CyclicQuery1, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult { return ctx.query<CyclicQuery2>(key); }

	QUERY_AUTO_CACHE_COPY
};

struct IMPLEMENT_QUERY(CyclicQuery2, u64) {
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

	QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF(false)
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

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(IdentityQuery);

	struct IMPLEMENT_QUERY(LeakQuery, u64) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			leaked_context = &ctx;
			ctx.query<UseLeakedContext>({ 1 });
			return key.value;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LeakQuery);

	struct IMPLEMENT_QUERY(UseLeakedContext, u64) {
		static auto provide(Context&, QKey key) -> PResult {
			use_leaked_query_happened = true;
			leaked_context->query<IdentityQuery>({ 1 });
			return key.value;
		}

		QUERY_AUTO_NO_CACHE
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

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(EmptyQuery);

DECLARE_QUERY(CallEmptyQueryNTimes, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CallEmptyQueryNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key.value; i++) ctx.query<EmptyQuery>({ i });
		return key.value;
	}

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallEmptyQueryNTimes);

DECLARE_QUERY(CallSideInputNTimes, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(CallSideInputNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key.value; i++) ctx.query<SideInput>({ i });
		return key.value;
	}

	QUERY_AUTO_NO_CACHE
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

private:
	NoctrKey(u32 value) noexcept: value{ value } {}

	NoctrKey(NoctrKey&&) noexcept                   = default;
	NoctrKey(const NoctrKey&) noexcept              = default;
	NoctrKey& operator=(const NoctrKey&) & noexcept = default;
	NoctrKey& operator=(NoctrKey&&) & noexcept      = default;
};

DECLARE_QUERY(DoNotCopyKeys, NoctrKey, u32, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DoNotCopyKeys, u32) {
	static auto provide(Context& context, const QKey& key) -> PResult {
		if (key.value == 0)
			return 0;
		else if (key.value == 1)
			return 1;
		else
			return context.query<DoNotCopyKeys>(NoctrKey::keyCreate(key.value - 1))
			     + context.query<DoNotCopyKeys>(NoctrKey::keyCreate(key.value - 2));
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DoNotCopyKeys);

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

	static auto load(KHash key_hash) -> LoadResult {
		last_hash = key_hash;
		return {};
	}

	static auto store([[maybe_unused]] KHash key_hash, PResult res, query::ACD) -> QResult {
		return res;
	}
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

	QUERY_AUTO_NO_CACHE
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

	QUERY_AUTO_NO_CACHE
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

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(NoQResultTest);

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
		TESTER_ADD_TEST(testNoKeyCopy);
		TESTER_ADD_TEST(stableHashTest);
		TESTER_ADD_TEST(testQueryResultExceptionsHandling);
	}

private:
	void simpleTest() {
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (1)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (2)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 0 }) == 0, "Bad query output (3)");
		assertTrue(query::entryPoint<FibonacciSum>(Key2{ 4 }) == 7, "Bad query output (4)");
		assertTrue(*query::entryPoint<ReferenceQuery>({ 88 }) == 88, "Bad query output (5)");
		assertTrue(
			*query::entryPoint<VectorReferenceQuery>({ 6 }) == std::vector<u64>{ 1, 2, 6 },
			"Bad query output (6)"
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

		assertThrows<base::Panic>(
			[&]() { graph.getNodeDeps<EmptyQuery>({ 1 }); }, "Query deps present before query call."
		);

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
		assertThrows<base::Panic>(
			[&]() { query::entryPoint<CallingEntryPoint>({ 1 }); },
			"Calling entry point from query did not panicked."
		);
#endif
	}

	template<class Query>
	void resultLifetimeTest() {
		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res2.validate());
		});

		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>({ 0 });
			ASSERT_TRUE(res2.validate());
		});
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
		// note: this test will change when proper cycle handling will
		// be introduced.
		assertThrows<base::NotYetImplemented>(
			[&]() { query::entryPoint<CyclicQuery1>({ 1 }); }, "Cycle detection did not throw."
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
			auto res1 = ctx.query<ConstructCacheTest>({ 10 });
			ASSERT_TRUE(res1.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 1);

			auto res2 = ctx.query<ConstructCacheTest>({ 10 });
			ASSERT_TRUE(res2.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 2);

			auto res3 = ctx.query<ConstructCacheTest>({ 20 });
			ASSERT_TRUE(res3.v == 20);
			ASSERT_TRUE(ConstructTo::construct_count == 3);
		});
	}

	void testConstructFromCRefCache() {
		ConstructToViaCRef::construct_count = 0;
		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<ConstructFromCRefCacheTest>({ 10 });
			ASSERT_TRUE(res1.v->v == 10);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 1);

			auto res2 = ctx.query<ConstructFromCRefCacheTest>({ 10 });
			ASSERT_TRUE(res2.v->v == 10);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 2);

			auto res3 = ctx.query<ConstructFromCRefCacheTest>({ 20 });
			ASSERT_TRUE(res3.v->v == 20);
			ASSERT_TRUE(ConstructToViaCRef::construct_count == 3);
		});
	}

	void testContextSanityCheck() {
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
		// Serialize the graph
		auto serialized_data = graph.serialize();

		auto deserialized_graph = query::internal::QueryGraph::deserialize(serialized_data);

		auto serialized_data2 = deserialized_graph.serialize();

		auto deserialized_graph2 = query::internal::QueryGraph::deserialize(serialized_data2);

		ASSERT_EQUAL(serialized_data.size(), serialized_data2.size());

		ASSERT_TRUE(graph.compare(deserialized_graph));
		ASSERT_TRUE(deserialized_graph2.compare(graph));

		query::entryPoint<Fibonacci>(Key1{ 30 });

		const auto& graph2 = query::Context::getState().getGraph();

		auto serialized_data3    = graph2.serialize();
		auto deserialized_graph3 = query::internal::QueryGraph::deserialize(serialized_data3);

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
		ASSERT_TRUE(hr1.hasValue());
		ASSERT_EQUAL(1, hr1.valueOrPanic());

		int                      temp_val = hr1.valueOrPanic();
		base::Optional<Ref<int>> opt1     = Ref<int>(&temp_val);
		ASSERT_TRUE(opt1.has_value());
		ASSERT_EQUAL(1, **opt1);

		query::QResult<std::string> hr2 = "Value";
		// base::Optional<std::string>        stolen_opt = std::move(hr2).optValueMove();
		// ASSERT_EQUAL("Value", stolen_opt);

		std::string                      info  = "Hello";
		query::QResult<std::string_view> whoa2 = std::string_view(info);
		ASSERT_TRUE(whoa2.hasValue());
	}

	void testNoKeyCopy() {
		assertEqual(
			query::entryPoint<DoNotCopyKeys>(NoctrKey::keyCreate(4)), 3, "Should be fibonacci(4) = 3"
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

		assertThrows<base::Panic>(
			[] { query::entryPoint<UsesQResultNoCatchTest>({ 1 }); },
			"QueryFailedException not thrown as expected"
		);

		assertThrows<base::Panic>(
			[] { query::entryPoint<NoQResultTest>({ 1 }); },
			"QueryFailedException not thrown as expected"
		);

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
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");

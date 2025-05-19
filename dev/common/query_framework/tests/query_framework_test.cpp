#include <query_framework/detail/query_graph/query_graph.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_input.hpp>
#include <query_framework/query_input_impl.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <base/anycast.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/stable_hashmap.hpp>

#include <sstream>

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

DECLARE_QUERY(Fibonacci, Key1, u64);
DECLARE_QUERY(FibonacciSum, Key2, u64);

/* * * *
 * Q1: *
 * * * */
struct IMPLEMENT_QUERY(Fibonacci, u64) {
	inline static std::map<query::QueryUnstableHash, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		if (key.v == 0)
			return 0;
		else if (key.v == 1)
			return 1;
		else
			return context.query<Fibonacci>({ key.v - 1 })
			     + context.query<Fibonacci>({ key.v - 2 });
	}

	static auto load(query::QueryUnstableHash key_hash) -> LoadResult {
		if (cache.contains(key_hash))
			return cache.at(key_hash);
		else
			return {};
	}

	static auto store(query::QueryUnstableHash key_hash, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key_hash, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(Fibonacci);


DECLARE_QUERY(FibonacciStringAutoCache, u64, std::string);

struct IMPLEMENT_QUERY(FibonacciStringAutoCache, std::string) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		return std::to_string(ctx.query<Fibonacci>(Key1{ key }));
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

	static auto load([[maybe_unused]] query::QueryUnstableHash key_hash) -> LoadResult {
		return {};
	}

	static auto store(
		[[maybe_unused]] query::QueryUnstableHash key_hash,
		PResult                                   res,
		[[maybe_unused]] query::ACD               acd
	) -> QResult {
		return QResult(res);
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibonacciSum);


DECLARE_QUERY(CallingEntryPoint, u64, u64);

struct IMPLEMENT_QUERY(CallingEntryPoint, u64) {
	static auto provide(Context&, QKey key) -> PResult {
		// call another query without context:
		return query::entryPoint<Fibonacci>({ key });
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallingEntryPoint);


DECLARE_QUERY(ReferenceQuery, u64, CRef<u64>);

struct IMPLEMENT_QUERY(ReferenceQuery, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key; }

	QUERY_AUTO_CACHE_REF
};

QUERY_IMPLEMENTATION_BOILERPLATE(ReferenceQuery);


DECLARE_QUERY(VectorReferenceQuery, u64, CRef<std::vector<u64>>);

struct IMPLEMENT_QUERY(VectorReferenceQuery, std::vector<u64>) {
	static auto provide(Context&, QKey key) -> PResult { return { 1, 2, key }; }

	QUERY_AUTO_CACHE_REF
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

DECLARE_QUERY(LifeTimeQueryStable, u64, Result);

struct IMPLEMENT_QUERY(LifeTimeQueryStable, Result) {
	static auto provide(Context&, QKey) -> PResult { return {}; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(LifeTimeQueryStable);

DECLARE_QUERY(LifeTimeQueryUnstable, u64, Result);

struct IMPLEMENT_QUERY(LifeTimeQueryUnstable, Result) {
	static auto provide(Context&, QKey) -> PResult { return {}; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(LifeTimeQueryUnstable);


DECLARE_QUERY(CyclicQuery1, u64, u64);
DECLARE_QUERY(CyclicQuery2, u64, u64);

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

DECLARE_QUERY(ConstructCacheTest, u64, ConstructTo);

struct IMPLEMENT_QUERY(ConstructCacheTest, ConstructFrom) {
	static auto provide(Context&, QKey key) -> PResult { return { key }; }

	QUERY_AUTO_CACHE_CONSTRUCT
};

QUERY_IMPLEMENTATION_BOILERPLATE(ConstructCacheTest);

namespace context_leak {
	query::Context* leaked_context = nullptr;

	// This is a flag used to check if context leak
	// detection actually took place, and that other
	// assertions did not prevent it
	bool use_leaked_query_happened = false;

	DECLARE_QUERY(IdentityQuery, u64, u64);
	DECLARE_QUERY(LeakQuery, u64, u64);
	DECLARE_QUERY(UseLeakedContext, u64, u64);

	struct IMPLEMENT_QUERY(IdentityQuery, u64) {
		static auto provide(Context&, QKey key) -> PResult { return key; }

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(IdentityQuery);

	struct IMPLEMENT_QUERY(LeakQuery, u64) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			leaked_context = &ctx;
			ctx.query<UseLeakedContext>(1);
			return key;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(LeakQuery);

	struct IMPLEMENT_QUERY(UseLeakedContext, u64) {
		static auto provide(Context&, QKey key) -> PResult {
			use_leaked_query_happened = true;
			leaked_context->query<IdentityQuery>(1);
			return key;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(UseLeakedContext);
}

DECLARE_QUERY_SIDE_INPUT(SideInput, u64);
IMPLEMENT_QUERY_SIDE_INPUT(SideInput);

DECLARE_QUERY(EmptyQuery, u64, u64);

struct IMPLEMENT_QUERY(EmptyQuery, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key; }

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(EmptyQuery);

DECLARE_QUERY(CallEmptyQueryNTimes, u64, u64);

struct IMPLEMENT_QUERY(CallEmptyQueryNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key; i++) ctx.query<EmptyQuery>(i);
		return key;
	}

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallEmptyQueryNTimes);

DECLARE_QUERY(CallSideInputNTimes, u64, u64);

struct IMPLEMENT_QUERY(CallSideInputNTimes, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		for (u64 i = 0; i < key; i++) ctx.query<SideInput>(i);
		return key;
	}

	QUERY_AUTO_NO_CACHE
};

QUERY_IMPLEMENTATION_BOILERPLATE(CallSideInputNTimes);

DECLARE_QUERY(CallEmptyQueryNTimesSideInput, u64, u64);

using query::utils::withContextCompute;
using query::utils::withContextDo;

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(autoCacheTest);
		TESTER_ADD_TEST(testConstructCache);
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
	}

private:
	void simpleTest() {
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (1)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (2)");
		assertTrue(query::entryPoint<Fibonacci>(Key1{ 0 }) == 0, "Bad query output (3)");
		assertTrue(query::entryPoint<FibonacciSum>(Key2{ 4 }) == 7, "Bad query output (4)");
		assertTrue(*query::entryPoint<ReferenceQuery>(88) == 88, "Bad query output (5)");
		assertTrue(
			*query::entryPoint<VectorReferenceQuery>(6) == std::vector<u64>{ 1, 2, 6 },
			"Bad query output (6)"
		);
	}

	void autoCacheTest() {
		// note: construct cache is tested in testConstructCache

		assertTrue(query::entryPoint<FibonacciStringAutoCache>(10) == "55", "Bad query output (5)");
		assertTrue(query::entryPoint<FibonacciStringAutoCache>(10) == "55", "Bad query output (6)");
	}

	void testDeps() {
		const auto& graph = query::Context::getGraph();

		assertThrows<base::Panic>(
			[&]() { graph.getNodeDeps<EmptyQuery>(1); }, "Query deps present before query call."
		);

		query::entryPoint<EmptyQuery>(1);
		auto deps = graph.getNodeDeps<EmptyQuery>(1);
		ASSERT_EQUAL(deps.size(), 1);

		assertThrows<base::Panic>(
			[&]() { graph.getNodeDeps<EmptyQuery>(2); }, "Query deps present before query call."
		);

		query::entryPoint<CallEmptyQueryNTimes>(10);
		auto deps2 = graph.getNodeDeps<CallEmptyQueryNTimes>(10);
		// 10 + 1 for the query itself:
		ASSERT_EQUAL(deps2.size(), 11);
		{
			auto deps2_filtered
				= graph.getNodeDepsFiltered<CallEmptyQueryNTimes>(10, EmptyQuery::getID());
			ASSERT_EQUAL(deps2_filtered.size(), 10);
		}
		{
			auto deps2_filtered
				= graph.getNodeDepsFiltered<CallEmptyQueryNTimes>(10, CallEmptyQueryNTimes::getID());
			ASSERT_EQUAL(deps2_filtered.size(), 1);
		}
		{
			auto deps2_filtered
				= graph.getNodeDepsFiltered<CallEmptyQueryNTimes>(10, Fibonacci::getID());
			ASSERT_EQUAL(deps2_filtered.size(), 0);
		}
	}

	void testSideInput() {
		const auto& graph = query::Context::getGraph();

		// we test that nothing breaks on multiple calls
		for (u64 i = 0; i < 10; i++) {
			query::entryPoint<CallSideInputNTimes>(10);
			auto deps = graph.getNodeDepsFiltered<CallSideInputNTimes>(10, SideInput::getID());
			ASSERT_EQUAL(deps.size(), 10);
		}
	}

	void entryPointSanityTest() {
		assertThrows<base::Panic>(
			[&]() { query::entryPoint<CallingEntryPoint>(1); },
			"Calling entry point from query did not panicked."
		);
	}

	template<class Query>
	void resultLifetimeTest() {
		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Query>(0);
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>(0);
			ASSERT_TRUE(res2.validate());
		});

		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<Query>(0);
			ASSERT_TRUE(res1.validate());

			auto res2 = ctx.query<Query>(0);
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
		assertTrue(Fibonacci::getData().name == "Fibonacci", "Bad query name (1)");
		assertTrue(FibonacciSum::getData().name == "FibonacciSum", "Bad query name (2)");
		assertTrue(
			FibonacciStringAutoCache::getData().name == "FibonacciStringAutoCache",
			"Bad query name (3)"
		);
		assertTrue(CallingEntryPoint::getData().name == "CallingEntryPoint", "Bad query name (4)");
		assertTrue(ReferenceQuery::getData().name == "ReferenceQuery", "Bad query name (5)");
		assertTrue(
			VectorReferenceQuery::getData().name == "VectorReferenceQuery", "Bad query name (6)"
		);
		assertTrue(
			LifeTimeQueryStable::getData().name == "LifeTimeQueryStable", "Bad query name (7)"
		);
		assertTrue(
			LifeTimeQueryUnstable::getData().name == "LifeTimeQueryUnstable", "Bad query name (8)"
		);
	}

	void cycleDetectionTest() {
		// note: this test will change when proper cycle handling will
		// be introduced.
		assertThrows<base::NotYetImplemented>(
			[&]() { query::entryPoint<CyclicQuery1>(1); }, "Cycle detection did not throw."
		);
	}

	void debugPrintTest() {
		const auto& graph = query::Context::getGraph();
		// just check if it compiles and don't throw
		std::stringstream s;
		graph.debugPrintForDrawing(s);
		graph.debugPrint(s);
	}

	void testConstructCache() {
		ConstructTo::construct_count = 0;
		withContextDo([&](query::Context& ctx) {
			auto res1 = ctx.query<ConstructCacheTest>(10);
			ASSERT_TRUE(res1.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 1);

			auto res2 = ctx.query<ConstructCacheTest>(10);
			ASSERT_TRUE(res2.v == 10);
			ASSERT_TRUE(ConstructTo::construct_count == 2);

			auto res3 = ctx.query<ConstructCacheTest>(20);
			ASSERT_TRUE(res3.v == 20);
			ASSERT_TRUE(ConstructTo::construct_count == 3);
		});
	}

	void testContextSanityCheck() {
		assertThrows<base::Panic>(
			[&]() { query::entryPoint<context_leak::LeakQuery>(1); },
			"Bad context usage not detected"
		);
		assertTrue(
			context_leak::use_leaked_query_happened,
			"Something else happened, the test is inconclusive"
		);
	}
};

TESTER_COMMON_MAIN("/common/query_framework/tests/");

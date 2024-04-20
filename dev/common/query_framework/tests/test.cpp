#include <tester/tester.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

struct Key1 {
	uint64_t       v;
	constexpr auto operator<=>(const Key1& oth) const = default;

	[[nodiscard]]
	base::HashT customPerfectHash() const {
		return v;
	}
};

struct Key2 {
	uint64_t v;

	[[nodiscard]]
	base::HashT customPerfectHash() const {
		return v;
	}
};

DECLARE_QUERY(Fibonacci, Key1, uint64_t);
DECLARE_QUERY(FibonacciSum, Key2, uint64_t);

/* * * *
 * Q1: *
 * * * */
struct ImplementationOf_Fibonacci: query::QueryImplementation<Fibonacci, u64> {
	inline static std::map<QKey, query::AddACD<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		if (key.v == 0)
			return 0;
		else if (key.v == 1)
			return 1;
		else
			return context.query<Fibonacci>({ key.v - 1 })
			     + context.query<Fibonacci>({ key.v - 2 });
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Fibonacci, "Q1");


DECLARE_QUERY(FibonacciStringAutoCache, uint64_t, std::string);

struct ImplementationOf_FibonacciStringAutoCache:
	  query::QueryImplementation<FibonacciStringAutoCache, std::string> {
	static auto provide(Context& ctx, QKey key) -> PResult {
		return std::to_string(ctx.query<Fibonacci>(Key1{ key }));
	}

	QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_FibonacciStringAutoCache, "Auto cache");

/* * * * *
 * Q2 a: *
 * * * * */
struct ImplementationOf_FibonacciSum: query::QueryImplementation<FibonacciSum, double> {
	static auto provide(Context& context, QKey key) -> PResult {
		double res = 0;
		for (uint64_t i = 0; i <= key.v; i++) res += double(context.query<Fibonacci>(Key1{ i }));
		return res;
	}

	static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

	static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd)
		-> QResult {
		return QResult(res);
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_FibonacciSum, "Q2 a");

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Query Test") {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(autoCacheTest);
	}

private:
	void simpleTest() {
		assert(query::queryEntryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (1)");
		assert(query::queryEntryPoint<Fibonacci>(Key1{ 10 }) == 55, "Bad query output (2)");
		assert(query::queryEntryPoint<Fibonacci>(Key1{ 0 }) == 0, "Bad query output (3)");
		assert(query::queryEntryPoint<FibonacciSum>(Key2{ 4 }) == 7, "Bad query output (4)");
	}

	void autoCacheTest() {
		assert(
			query::queryEntryPoint<FibonacciStringAutoCache>(10) == "55", "Bad query output (5)"
		);
		assert(
			query::queryEntryPoint<FibonacciStringAutoCache>(10) == "55", "Bad query output (6)"
		);
	}
};

TESTER_COMMON_MAIN("/common/query_framework/tests/");

#include <tester/tester.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

struct Key1 {
	uint64_t       v;
	constexpr auto operator<=>(const Key1& oth) const = default;
};

template<>
struct std::hash<Key1> {
	std::size_t operator()([[maybe_unused]] const Key1& key) const { return key.v; }
};

struct Key2 {
	uint64_t v;
};

template<>
struct std::hash<Key2> {
	std::size_t operator()([[maybe_unused]] const Key2& key) const { return key.v; }
};

DECLARE_QUERY(Fibonacci, Key1, uint64_t);
DECLARE_QUERY(FibonacciSum, Key2, uint64_t);

/* * * *
 * Q1: *
 * * * */
struct ImplementationOf_Fibonacci: query::QueryImplementation<Fibonacci, uint64_t> {
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

/* * * * *
 * Q2 a: *
 * * * * */
struct ImplementationOf_FibonacciSum: query::QueryImplementation<FibonacciSum, double> {
	static auto provide(Context& context, QKey key) -> PResult {
		double res = 0;
		for (uint64_t i = 0; i <= key.v; i++) {
			res += double(context.query<Fibonacci>(Key1(i)));
		}
		return res;
	}

	static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

	static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd) -> QResult { return QResult(res); }
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_FibonacciSum, "Q2 a");

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Query Test") { TESTER_ADD_TEST(simpleTest); }

private:
	void simpleTest() {
		assert(query::queryEntryPoint<Fibonacci>(Key1(10)) == 55, "Bad query output (1)");
		assert(query::queryEntryPoint<Fibonacci>(Key1(10)) == 55, "Bad query output (2)");
		assert(query::queryEntryPoint<Fibonacci>(Key1(0)) == 0, "Bad query output (3)");
		assert(query::queryEntryPoint<FibonacciSum>(Key2(4)) == 7, "Bad query output (4)");
	}
};

TESTER_COMMON_MAIN("/common/query_framework/tests/");

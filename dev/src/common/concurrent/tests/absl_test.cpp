// #include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/collections/absl_hash_map.hpp>
#include <concurrent/race_tester/race_tester.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <random>
#include <set>
#include <unordered_map>

class ConcurrentTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcurrentTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpl);

	
	}

protected:
	void beforeAll() override { concurrent::worker::setWorkerCount(4); }

private:
	void simpl() {
		concurrent::AbslConHashMap<i64, i64> map;
		auto x = map.put(1, 10);

		absl::node_hash_map<i64, i64> reference;
		reference.insert({1, 10});
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");

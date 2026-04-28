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
		auto y = map.maybePut(i64(1), 10);
		auto z = map.maybePutAndUpdate(i64(1), 10, [](base::CRef<i64>) {});
		auto w = map.contains(1);
		auto v = map.extract(3);
		map.update(1, 20);
		auto t = map.getAllKeyValuePairs();

		absl::node_hash_map<i64, i64> reference;
		reference.insert({1, 10});
		auto hm = reference.extract(1);
		auto hm2 = reference.erase(1);

		std::vector<std::pair<const i64, i64>> result;
		for (const auto& kv : reference) {
					result.push_back(kv);
		}
		
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");

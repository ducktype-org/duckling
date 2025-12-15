#include <tester/tester.hpp>
#include <concurrent/collections/hash_map.hpp>

class ConcurrentTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcurrentTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::setWorkerCount(4);

		TESTER_ADD_TEST(hashMapSingleThreadTest1);
	}

private:
	/** 
	 * Simple single-threaded test of concurrent::HashMap.
	 */
	void hashMapSingleThreadTest1() {
		concurrent::HashMap<int, int> map;
		
		map.put(1, 10);
		map.put(2, 20);
		map.put(3, 30);
		ASSERT_TRUE(map.getCopy(1) == 10);
		ASSERT_TRUE(map.getCopy(2) == 20);
		ASSERT_TRUE(map.getCopy(3) == 30);

		ASSERT_TRUE(map.contains(1));
		ASSERT_TRUE(map.contains(2));
		ASSERT_TRUE(map.contains(3));
		ASSERT_TRUE(!map.contains(4));

		ASSERT_TRUE(map.atMaybe(2).has_value());
		ASSERT_TRUE(map.atMaybe(4).empty());
		ASSERT_TRUE(map.atMaybe(5).empty());


		map.update(2, 25);
		ASSERT_TRUE(map.getCopy(2) == 25);

		auto val_ref = map.atMaybe(3).value();
		*val_ref = 35;
		ASSERT_TRUE(map.getCopy(3) == 35);
		ASSERT_TRUE(*map.atMaybe(3).value() == 35);


		map.tryPutAndUpdate(4, 40, [](int& v) {
			v += 5;
		});
		ASSERT_TRUE(map.getCopy(4) == 45);
		map.tryPutAndUpdate(1, 100, [](int& v) {
			v += 5;
		});
		ASSERT_TRUE(map.getCopy(1) == 15);

		map.tryPut(5, 50);
		ASSERT_TRUE(map.getCopy(5) == 50);
		map.tryPut(5, 500);
		ASSERT_TRUE(map.getCopy(5) == 50);
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");

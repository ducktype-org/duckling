#include <base/collections/object_pool.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

// A simple test type that tracks its ID for PASS_ID_TO_CONSTRUCTOR testing.
struct PoolItem {
	u64  id;
	i32  value;
	bool alive = true;

	PoolItem() = default;
	explicit PoolItem(u64 id, i32 value): id(id), value(value) {}
};

class ObjectPoolTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ObjectPoolTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(addAndGetTest);
		TESTER_ADD_TEST(recyclingTest);
		TESTER_ADD_TEST(removeAndReaddTest);
		TESTER_ADD_TEST(iteratorSkipsFreeSlotsTest);
		TESTER_ADD_TEST(emptyPoolIteratorTest);
		TESTER_ADD_TEST(maybeGetMissingTest);
		TESTER_ADD_TEST(maybeGetAfterRemoveTest);
	}

private:
	void addAndGetTest() {
		base::StableObjectPool<PoolItem, u64, false, true> pool;

		auto id1 = pool.add(42);
		auto id2 = pool.add(99);

		// IDs should be sequential starting from 0.
		ASSERT_EQUAL(id1, 0ULL);
		ASSERT_EQUAL(id2, 1ULL);

		// Retrieved items should have the correct values (Ref<T> is pointer-like).
		ASSERT_EQUAL(pool.get(id1)->value, 42);
		ASSERT_EQUAL(pool.get(id1)->id, 0ULL);

		ASSERT_EQUAL(pool.get(id2)->value, 99);
		ASSERT_EQUAL(pool.get(id2)->id, 1ULL);

		// maybeGet should also work.
		auto opt1 = pool.maybeGet(id1);
		ASSERT_TRUE(opt1.has_value());
		ASSERT_EQUAL(opt1.value()->value, 42);

		auto opt2 = pool.maybeGet(id2);
		ASSERT_TRUE(opt2.has_value());
		ASSERT_EQUAL(opt2.value()->value, 99);
	}

	void recyclingTest() {
		// SHOULD_RECYCLE=true + PASS_ID_TO_CONSTRUCTOR=true
		base::StableObjectPool<PoolItem, u64, true, true> pool;

		auto id0 = pool.add(10);
		auto id1 = pool.add(20);
		auto id2 = pool.add(30);

		ASSERT_EQUAL(id0, 0ULL);
		ASSERT_EQUAL(id1, 1ULL);
		ASSERT_EQUAL(id2, 2ULL);

		// Remove id1 (the middle one).
		pool.remove(id1);

		// Adding a new item should recycle id1 (FIFO — first freed is reused first).
		auto id3 = pool.add(99);
		ASSERT_EQUAL(id3, 1ULL);  // Should reuse id1.

		ASSERT_EQUAL(pool.get(id3)->value, 99);
		ASSERT_EQUAL(pool.get(id3)->id, 1ULL);

		// Original items still accessible.
		ASSERT_EQUAL(pool.get(id0)->value, 10);
		ASSERT_EQUAL(pool.get(id2)->value, 30);

		// Remove id0, then check FIFO order: id1 was already recycled, now id0 should be next.
		pool.remove(id0);
		auto id4 = pool.add(77);
		ASSERT_EQUAL(id4, 0ULL);  // id0 recycled FIFO.
		ASSERT_EQUAL(pool.get(id4)->value, 77);
	}

	void removeAndReaddTest() {
		base::StableObjectPool<PoolItem, u64, true, true> pool;

		auto id = pool.add(5);
		ASSERT_EQUAL(pool.get(id)->value, 5);

		pool.remove(id);

		// maybeGet should return empty after removal.
		auto opt = pool.maybeGet(id);
		ASSERT_TRUE(!opt.has_value());

		// Re-add should recycle the id.
		auto new_id = pool.add(42);
		ASSERT_EQUAL(new_id, id);  // Reuses the same ID.

		ASSERT_EQUAL(pool.get(new_id)->value, 42);
	}

	void iteratorSkipsFreeSlotsTest() {
		base::StableObjectPool<PoolItem, u64, true, true> pool;

		pool.add(1);   // id=0
		pool.add(2);   // id=1
		pool.add(3);   // id=2
		pool.add(4);   // id=3

		pool.remove(1);  // remove id=1
		pool.remove(2);  // remove id=2

		// Iterator should only yield valid objects (ids 0 and 3).
		i32 sum = 0;
		u64 count = 0;
		for (auto& item: pool) {
			sum += item.value;
			count++;
		}

		ASSERT_EQUAL(count, 2ULL);
		ASSERT_EQUAL(sum, 5);  // 1 + 4 = 5
	}

	void emptyPoolIteratorTest() {
		base::StableObjectPool<PoolItem, u64, true, true> pool;

		u64 count = 0;
		for ([[maybe_unused]] auto& item: pool) {
			count++;
		}
		ASSERT_EQUAL(count, 0ULL);
	}

	void maybeGetMissingTest() {
		base::StableObjectPool<PoolItem, u64, false, true> pool;

		// maybeGet on an ID that was never added should return empty.
		auto opt = pool.maybeGet(999);
		ASSERT_TRUE(!opt.has_value());
	}

	void maybeGetAfterRemoveTest() {
		base::StableObjectPool<PoolItem, u64, true, true> pool;

		auto id = pool.add(7);
		pool.remove(id);

		auto opt = pool.maybeGet(id);
		ASSERT_TRUE(!opt.has_value());
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");

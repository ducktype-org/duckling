#include <base/extend_cpp/vector_utils.hpp>

#include <tester/tester.hpp>

class VectorUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VectorUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(appendToVectorTest);
		TESTER_ADD_TEST(filterVectorInPlaceTest);
		TESTER_ADD_TEST(deduplicateByTest);
	}

	void appendToVectorTest() {
		std::vector<int> dest = { 1, 2, 3 };
		std::vector<int> src  = { 4, 5, 6 };

		base::appendToVector(dest, src);

		assertTrue(dest.size() == 6, "Expected dest size to be 6 after appending");
		assertTrue(src.size() == 3, "Expected src size to be 3 after appending (not modified)");

		std::vector<int> expected = { 1, 2, 3, 4, 5, 6 };
		assertTrue(
			dest == expected, "Expected dest to contain the combined elements of dest and src"
		);
	}

	void filterVectorInPlaceTest() {
		std::vector<int> vec = { 1, 2, 3, 4, 5, 6 };

		base::filterVectorInPlace(vec, [](int item) {
			return item % 2 == 0;  // Keep only even numbers
		});

		std::vector<int> expected = { 2, 4, 6 };
		ASSERT_EQUAL_PRINT(vec.size(), expected.size());
		assertTrue(vec == expected, "Expected vec to contain only even numbers after filtering");
	}

	void deduplicateByTest() {
		std::vector<int> vec = { 1, 2, 3, 4, 5, 6 };

		base::deduplicateBy(vec, [](auto elem) { return elem % 3; });
		std::vector<int> expected = { 1, 2, 3 };
		ASSERT_EQUAL_PRINT(vec.size(), expected.size());
		assertTrue(vec == expected, "Expected vec to contain only even numbers after filtering");
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");

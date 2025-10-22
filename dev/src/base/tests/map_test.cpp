#include <base/collections/stable_hashmap.hpp>

#include <tester/tester.hpp>
#include <random>

template<usize Size>
struct BigObject final {
    u64 data[Size] = {};
    BigObject(u64 a): data{a} {}

    bool operator==(const BigObject& other) const {
        for (usize i = 0; i < Size; i++) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
};
template<usize N>
struct std::hash<BigObject<N>> {
    size_t operator()(const BigObject<N>& obj) const noexcept {
        return obj.data[0];
    }
};


class MapTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MapTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
        TESTER_ADD_TEST(basicTest<0>);
        TESTER_ADD_TEST(basicTest<2>);
        TESTER_ADD_TEST(basicTest<10>);
        TESTER_ADD_TEST(basicTest<100>);
        TESTER_ADD_TEST(basicTest<10000>);
        TESTER_ADD_TEST(basicTest<1000000>);
    }

    template<u64 count>
	void basicTest() {
        std::minstd_rand rng(42);

        base::StableHashMap20<BigObject<13>, BigObject<16>> map;
        // base::HashMap<BigObject<13>, BigObject<16>> map;

        u64 base_result = 0;

        for (u64 i = 0; i < count; i++) {
            auto v = rng();
            map.put(v, v * 10);

            BigObject<16> val(0);
            auto maybe_val = map.atMaybe(rng());
            if (maybe_val.has_value()) {
                val = **maybe_val;
                base_result += val.data[0];
            }
        }

        u64 loop_count = 0;
        for (auto& [key, value]: map) {
            loop_count++;
            ASSERT_EQUAL(value.data[0], key.data[0] * 10);
        }
        // ASSERT_TRUE(loop_count == count);
        std::cerr << "loop_count" << loop_count << "\n";
        std::cerr << "base_result" << base_result << "\n";
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");

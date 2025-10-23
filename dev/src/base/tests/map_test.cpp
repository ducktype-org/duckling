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
        TESTER_ADD_TEST(basicTest<100000>);
    }

    template<u64 count>
	void basicTest() {
        u64 base_loop_count = 0;
        u64 base_result = 0;
        {
            std::minstd_rand rng(42);

            base::StableHashMap20<BigObject<13>, BigObject<16>> map;

            for (u64 i = 0; i < count; i++) {
                auto v = rng() % 1000000;
                map.put(v, v * 10);
            
                for (int j = 0; j < 3; j++) {
                    auto maybe_val = map.atMaybe(rng() % 1000000);
                    if (maybe_val.has_value()) {
                        auto val = **maybe_val;
                        base_result += val.data[0];
                    }
                }
            }

            
            for (auto& [key, value]: map) {
                base_loop_count++;
                ASSERT_EQUAL(value.data[0], key.data[0] * 10);
            }
        }

        u64 std_loop_count = 0;
        u64 std_result = 0;
        {
            std::minstd_rand rng(42);

            std::unordered_map<BigObject<13>, BigObject<16>> map;

            for (u64 i = 0; i < count; i++) {
                auto v = rng() % 1000000;
                map.emplace(BigObject<13>(v), BigObject<16>(v * 10));
            
                for (int j = 0; j < 3; j++) {
                    auto it = map.find(BigObject<13>(rng() % 1000000));
                    if (it != map.end()) {
                        auto val = it->second;
                        std_result += val.data[0];
                    }
                }
            }

            
            for (auto& [key, value]: map) {
                std_loop_count++;
                ASSERT_EQUAL(value.data[0], key.data[0] * 10);
            }
        }

        ASSERT_EQUAL(base_loop_count, std_loop_count);
        ASSERT_EQUAL(base_result, std_result);
        
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");

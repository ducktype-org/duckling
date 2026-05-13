#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/hashmap.hpp>
#include <vm/utils/persistent/memory.hpp>
#include <vm/utils/persistent/vector.hpp>

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class PersistentStlTester: public tester::TestSuite {
	template<typename Val>
	struct dummyPersistentVec {
		std::unordered_map<usize, std::vector<Val>> dict = { 0, {} };

		usize emplace(const std::vector<Val>& inp) {
			for (auto& [id, vec]: dict)
				if (vec == inp) return id;

			usize new_id = dict.size();
			dict.emplace(new_id, inp);

			return new_id;
		}

		usize push(usize id, const Val& v) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy = dict.at(id);
			cpy.push_back(v);

			return emplace(cpy);
		}

		usize pop(usize id, usize how_many = 1) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy = dict.at(id);

			CORE_ASSERT(cpy.size() >= how_many, "vec must be big enough");
			for (usize i = 0; i < how_many; i++) cpy.pop_back();

			return emplace(cpy);
		}

		usize change(usize id, usize idx, const Val& v) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy = dict.at(id);
			CORE_ASSERT(idx < cpy.size(), "we require valid idx");
			cpy.at(idx) = v;

			return emplace(cpy);
		}
	};

	template<typename Key, typename Val>
	struct dummyPersistentMap {
		std::unordered_map<usize, base::HashMap<Key, Val>> dict = { 0, {} };

		usize emplace(const base::HashMap<Key, Val>& inp) {
			for (auto& [id, map]: dict)
				if (map == inp) return id;

			usize new_id = dict.size();
			dict.emplace(new_id, inp);

			return new_id;
		}

		usize insert(usize id, const Key& k, const Val& v) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy  = dict.at(id);
			cpy.at(k) = v;

			return emplace(cpy);
		}

		usize erase(usize id, const Key& k) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy = dict.at(id);
			cpy.erase(k);

			return emplace(cpy);
		}

		bool contains(usize id, const Key& k) const {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			return dict.at(id).contains(k);
		}

		std::pair<bool, usize> emplace(usize id, const Key& k, const Val& v) {
			CORE_ASSERT(dict.contains(id), "must be valid id");
			auto cpy      = dict.at(id);
			auto [suc, _] = cpy.emplace(k, v);

			if (!suc) return { false, id };

			usize new_id = emplace(cpy);
			return { true, new_id };
		}
	};

#undef TESTER_CLASS
#define TESTER_CLASS PersistentStlTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBijective);
		TESTER_ADD_TEST(testMemory);
		TESTER_ADD_TEST(testVector);
		TESTER_ADD_TEST(testHashMap);
	}

	void testBijective() {
		vm::persistent::detail::BijectiveMap<base::StrID, usize> dir{};

		std::array<base::StrID, 10> vals;
		for (usize i = 0; i < vals.size(); i++) {
			vals.at(i) = base::StrID("val" + std::to_string(i));
			dir.emplaceByLeft(vals.at(i), i);
		}

		for (usize i = 0; i < 10; i++) {
			auto& rght = dir.atLeft(vals.at(i));
			auto& left = dir.atRight(i);


			ASSERT_EQUAL(i, rght);
			ASSERT_EQUAL(vals.at(i), left);

			auto maybe_rght = dir.atLeftOpt(vals.at(i));
			auto maybe_left = dir.atRightOpt(i);

			ASSERT_TRUE(maybe_left.has_value());
			ASSERT_TRUE(maybe_rght.has_value());

			ASSERT_EQUAL(i, *maybe_rght);
			ASSERT_EQUAL(vals.at(i), *maybe_left);

			auto [inserted, refR] = dir.emplaceByLeft(vals.at(i), 11);
			ASSERT_TRUE(!inserted);
		}

		base::StrID new_val("new_val");
		auto [inserted, refR] = dir.emplaceByLeft(new_val, 11);

		ASSERT_TRUE(inserted);
	}

	void testMemory() {
		using namespace vm::persistent;
		Memory mem{};

		auto checker = [&](
						   MemoryStateID state, const std::vector<std::pair<usize, usize>>& expected
					   ) -> void { 
						ASSERT_EQUAL(expected, mem.toVec(state));
						MemoryStateView view(mem, state);
						ASSERT_EQUAL(expected, view.toVec());
						ASSERT_EQUAL(expected.size(), view.size());
						
						for (auto [idx, var]: expected) {
							ASSERT_TRUE(view.contains(idx));
							ASSERT_TRUE(view.atMaybe(idx).has_value());
							ASSERT_EQUAL(view[idx], var);
						}
					};

		auto empt = Memory::EMPTY;
		checker(empt, {});

		auto op01 = mem.set(empt, 1, 5);
		checker(op01, { { 1, 5 } });

		auto op02 = mem.set(op01, 2, 7);
		checker(op02, { { 1, 5 }, { 2, 7 } });

		auto op03 = mem.set(empt, 2, 7);
		checker(op03, { { 2, 7 } });

		auto op04 = mem.set(op03, 1, 5);
		checker(op04, { { 1, 5 }, { 2, 7 } });

		auto op05 = mem.setMultiple(op03, { { 1, 5 }, { 2, 7 } });
		checker(op05, { { 1, 5 }, { 2, 7 } });

		ASSERT_EQUAL(op02, op04);
		ASSERT_EQUAL(op02, op05);

		auto op06 = mem.set(op05, Memory::IDX_END - 1, 15);
		checker(op06, { { 1, 5 }, { 2, 7 }, { Memory::IDX_END - 1, 15 } });

		auto op07 = mem.setMultiple(op05, { { 1'410, 512 }, { 2'137, 67 }, { 8'008'135, 69 } });
		checker(op07, { { 1, 5 }, { 2, 7 }, { 1'410, 512 }, { 2'137, 67 }, { 8'008'135, 69 } });

		auto op08 = mem.eraseRange(op07, 1'000, 2'138);
		checker(op08, { { 1, 5 }, { 2, 7 }, { 8'008'135, 69 } });

		auto           diff = mem.getDiff(op07, op08);
		decltype(diff) exp  = { { 1'410, 512, std::nullopt }, { 2'137, 67, std::nullopt } };
		CORE_ASSERT(diff == exp, "Expecting two elements missing");

		auto op09 = mem.set(empt, 8'484, 173);
		checker(op09, { { 8'484, 173 } });

		auto op10 = mem.merge(op09, op08, [](usize, usize l, usize) -> base::Optional<usize> {
			return l;
		});
		checker(op10, { { 1, 5 }, { 2, 7 }, { 8'484, 173 }, { 8'008'135, 69 } });
	}

	void testVector() {
		using namespace vm::persistent;

		Vector<std::string> vec;
		auto checker = [&](VectorStateID state, std::vector<std::string> expected) -> void {
			auto size = expected.size();
			ASSERT_EQUAL(expected.size(), vec.size(state));
			auto view = vec.view(state, 0, size);
			ASSERT_EQUAL(expected, view);

			for (usize i = 0; i < size; i++) ASSERT_EQUAL(expected[i], vec.access(state, i));
		};

		auto empt = Vector<std::string>::EMPTY;
		checker(empt, {});

		auto op01 = vec.push(empt, std::string("val01"));
		checker(op01, { "val01" });

		auto op02 = vec.push(op01, std::string("val02"));
		checker(op02, { "val01", "val02" });

		auto op03 = vec.push(op02, std::string("val03"));
		checker(op03, { "val01", "val02", "val03" });

		auto op04 = vec.getPrefix(op03, 2);
		checker(op04, { "val01", "val02" });

		auto op05 = vec.pop(op03);
		checker(op05, { "val01", "val02" });

		auto op06 = vec.push(op03, std::string("val05"));
		checker(op06, { "val01", "val02", "val03", "val05" });

		auto op07 = vec.push(op06, std::string("val06"));
		checker(op07, { "val01", "val02", "val03", "val05", "val06" });

		auto op08 = vec.change(op07, 2, std::string("val07"));
		checker(op08, { "val01", "val02", "val07", "val05", "val06" });

		auto op09 = vec.change(op08, 1, std::string("val08"));
		checker(op09, { "val01", "val08", "val07", "val05", "val06" });

		auto op10 = vec.push(op09, std::string("val09"));
		checker(op10, { "val01", "val08", "val07", "val05", "val06", "val09" });

		auto op11 = vec.pop(op10, 2);
		checker(op11, { "val01", "val08", "val07", "val05" });

		auto op12 = vec.change(op11, 0, std::string("val11"));
		checker(op12, { "val11", "val08", "val07", "val05" });

		auto op13 = vec.change(op12, 1, std::string("val02"));
		checker(op13, { "val11", "val02", "val07", "val05" });

		auto op14 = vec.change(op13, 0, std::string("val01"));
		checker(op14, { "val01", "val02", "val07", "val05" });

		auto op15 = vec.change(op14, 2, std::string("val03"));
		checker(op15, { "val01", "val02", "val03", "val05" });

		auto op16 = vec.getPrefix(op15, 3);
		checker(op16, { "val01", "val02", "val03" });

		ASSERT_EQUAL(op16, op03);
		ASSERT_EQUAL(op02, op04);
		ASSERT_EQUAL(op05, op04);
	}

	void testHashMap() {
		using namespace vm::persistent;

		HashMap<std::string, std::string> map;
		auto                              checker
			= [&](HashMapStateID state, base::HashMap<std::string, std::string> expected) -> void {
			ASSERT_EQUAL(expected.size(), map.size(state));
			auto map_copy = map.toMap(state);

			for (auto& [key, val]: expected) {
				CORE_ASSERT(
					map_copy.contains(key) && map.contains(state, key),
					"map should contain all of expected values"
				);
				CORE_ASSERT(
					map_copy.at(key) == val, "values should be equal in both copy and database"
				);

				CORE_ASSERT(
					map.access(state, key) == val, "values should be equal in both copy and database"
				);
			}

			for (auto& [key, val]: map_copy) {
				CORE_ASSERT(
					expected.contains(key) && map.contains(state, key),
					"map should contain all of expected values"
				);
				CORE_ASSERT(
					expected.at(key) == val && map.access(state, key) == val,
					"values should be equal in both copy and database"
				);
			}
		};


		auto empt = HashMap<std::string, std::string>::EMPTY;
		checker(empt, {});

		auto op01 = map.insert(empt, "key1", "val1");
		checker(op01, { { "key1", "val1" } });

		auto op02 = map.insert(op01, "key2", "val2");
		checker(op02, { { "key1", "val1" }, { "key2", "val2" } });

		auto op03 = map.insert(op02, "key3", "val2");
		checker(op03, { { "key1", "val1" }, { "key2", "val2" }, { "key3", "val2" } });

		auto op04 = map.erase(op03, "key2");
		checker(op04, { { "key1", "val1" }, { "key3", "val2" } });

		auto [success, op05] = map.emplace(op04, "key2", "val5");
		ASSERT_EQUAL(success, true);
		checker(op05, { { "key1", "val1" }, { "key2", "val5" }, { "key3", "val2" } });

		auto [success_2, op06] = map.emplace(op05, "key2", "val6");
		ASSERT_EQUAL(success_2, false);
		checker(op06, { { "key1", "val1" }, { "key2", "val5" }, { "key3", "val2" } });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/utils/");

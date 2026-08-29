#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/dummy/hashmap.hpp>
#include <vm/utils/persistent/dummy/vector.hpp>
#include <vm/utils/persistent/hashmap.hpp>
#include <vm/utils/persistent/memory.hpp>
#include <vm/utils/persistent/tree.hpp>
#include <vm/utils/persistent/vector.hpp>

#include <array>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

class PersistentStlTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PersistentStlTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBijective);
		TESTER_ADD_TEST(testMemory);
		TESTER_ADD_TEST(testMemoryAccess);
		TESTER_ADD_TEST(testSegmentTreePaths);
		TESTER_ADD_TEST(testSegmentTreeReadOnlyRangeTraversal);
		TESTER_ADD_TEST(testVector);
		TESTER_ADD_TEST(testHashMap);
		TESTER_ADD_TEST(testRandomVector);
		TESTER_ADD_TEST(testRandomHashMap);
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

		ASSERT_TRUE(dir.atLeftOpt(base::StrID("non_existent")).empty());
		ASSERT_TRUE(dir.atRightOpt(999).empty());
	}

	void testMemory() {
		using namespace vm::persistent;
		Memory mem{};

		auto checker = [&](MemoryStateID                               state,
		                   const std::vector<std::pair<usize, usize>>& expected) -> void {
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

		auto path = *mem.getPathTo(op02, 1, Memory::Dir::Rght);
		ASSERT_EQUAL(1, path.getIdx());
		ASSERT_TRUE(path.moveToValid(Memory::Dir::Rght));
		ASSERT_EQUAL(2, path.getIdx());
		ASSERT_TRUE(path.moveToValid(Memory::Dir::Left));
		ASSERT_EQUAL(1, path.getIdx());

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

		auto diff_same = mem.getDiff(op07, op07);
		ASSERT_TRUE(diff_same.empty());

		auto op09 = mem.set(empt, 8'484, 173);
		checker(op09, { { 8'484, 173 } });

		auto op10 = mem.merge(op09, op08, [](usize, usize l, usize) -> base::Optional<usize> {
			return l;
		});
		checker(op10, { { 1, 5 }, { 2, 7 }, { 8'484, 173 }, { 8'008'135, 69 } });

		auto op11 = mem.merge(op10, empt, [](usize, usize l, usize) -> base::Optional<usize> {
			return l;
		});
		checker(op11, { { 1, 5 }, { 2, 7 }, { 8'484, 173 }, { 8'008'135, 69 } });

		auto op12 = mem.merge(op10, empt, [](usize, usize l, usize) -> base::Optional<usize> {
			return l;
		});
		checker(op12, { { 1, 5 }, { 2, 7 }, { 8'484, 173 }, { 8'008'135, 69 } });

		ASSERT_EQUAL(op10, op11);
		ASSERT_EQUAL(op11, op12);

		auto op13 = mem.erase(op02, 1);
		checker(op13, { { 2, 7 } });
		ASSERT_EQUAL(op13, op03);

		auto op14 = mem.erase(op13, 2);
		checker(op14, {});
		ASSERT_EQUAL(op14, empt);

		auto op15 = mem.slice(op07, 1'000, 2'138);
		checker(op15, { { 1'410, 512 }, { 2'137, 67 } });

		auto op16 = mem.set(op01, 1, 999);
		checker(op16, { { 1, 999 } });

		auto op17 = mem.merge(op01, op16, [](usize, usize, usize r) -> base::Optional<usize> {
			return r;
		});
		checker(op17, { { 1, 999 } });
		ASSERT_EQUAL(op17, op16);
	}

	void testMemoryAccess() {
		using namespace vm::persistent;

		Memory memory;
		auto   state = memory.set(Memory::EMPTY, 10, 100);
		state        = memory.set(state, 20, 200);

		ASSERT_TRUE(memory.access(state, 10).has_value());
		ASSERT_EQUAL(100, *memory.access(state, 10));
		ASSERT_TRUE(memory.access(state, 20).has_value());
		ASSERT_EQUAL(200, *memory.access(state, 20));

		MemoryStateView                      view(memory, state);
		std::vector<std::pair<usize, usize>> entries;
		for (auto entry: view) entries.emplace_back(entry);
		const std::vector<std::pair<usize, usize>> expected{ { 10, 100 }, { 20, 200 } };
		ASSERT_EQUAL(expected, entries);

		ASSERT_TRUE(memory.getPathTo(state, 5).empty());
		ASSERT_TRUE(memory.getPathTo(state, 100'000).empty());

		for (auto dir: { Memory::Dir::Left, Memory::Dir::Rght }) {
			auto path = memory.getPathTo(state, 10, dir);
			ASSERT_TRUE(path.has_value());
			ASSERT_EQUAL(10, path->getIdx());
			ASSERT_EQUAL(100, path->getValue());

			path = memory.getPathTo(state, 20, dir);
			ASSERT_TRUE(path.has_value());
			ASSERT_EQUAL(20, path->getIdx());
			ASSERT_EQUAL(200, path->getValue());
		}

		auto left_path = memory.getPathTo(state, 15, Memory::Dir::Left);
		ASSERT_TRUE(left_path.has_value());
		ASSERT_EQUAL(10, left_path->getIdx());
		ASSERT_EQUAL(100, left_path->getValue());

		auto right_path = memory.getPathTo(state, 15, Memory::Dir::Rght);
		ASSERT_TRUE(right_path.has_value());
		ASSERT_EQUAL(20, right_path->getIdx());
		ASSERT_EQUAL(200, right_path->getValue());

		ASSERT_TRUE(memory.getPathTo(state, 5, Memory::Dir::Left).empty());
		left_path = memory.getPathTo(state, 5, Memory::Dir::Rght);
		ASSERT_TRUE(left_path.has_value());
		ASSERT_EQUAL(10, left_path->getIdx());
		ASSERT_EQUAL(100, left_path->getValue());

		left_path = memory.getPathTo(state, 100'000, Memory::Dir::Left);
		ASSERT_TRUE(left_path.has_value());
		ASSERT_EQUAL(20, left_path->getIdx());
		ASSERT_EQUAL(200, left_path->getValue());
		ASSERT_TRUE(memory.getPathTo(state, 100'000, Memory::Dir::Rght).empty());
	}

	void testSegmentTreePaths() {
		using Tree = vm::persistent::detail::SegmentTree;
		using Dir  = Tree::Dir;

		Tree       tree;
		const auto empty = Tree::EMPTY;

		ASSERT_TRUE(!tree.getPathTo(empty, 0).has_value());

		const auto single = tree.emplaceLeaf(10, 100);
		ASSERT_EQUAL(1, tree.getSize(single));
		const auto single_range = tree.getRange(single);
		ASSERT_EQUAL(10, single_range.first);
		ASSERT_EQUAL(11, single_range.second);

		auto path = tree.getPathTo(single, 10);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(10, path->getIdx());
		ASSERT_EQUAL(100, path->getValue());
		path = tree.getPathTo(single, 10);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(!path->moveToValid(Dir::Left));
		path = tree.getPathTo(single, 10);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(!path->moveToValid(Dir::Rght));

		ASSERT_TRUE(!tree.getPathTo(single, 5).has_value());
		path = tree.getPathTo(single, 5, Dir::Left);
		ASSERT_TRUE(!path.has_value());
		path = tree.getPathTo(single, 5, Dir::Rght);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(10, path->getIdx());
		path = tree.getPathTo(single, 15, Dir::Left);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(10, path->getIdx());
		path = tree.getPathTo(single, 15, Dir::Rght);
		ASSERT_TRUE(!path.has_value());

		const std::deque<usize> indices = { 0, 10, 20, Tree::IDX_END - 1 };
		const auto              root
			= tree.reconstructLeaves(empty, indices, [&tree](usize idx, base::Optional<usize>) {
				  return tree.emplaceLeaf(idx, idx + 1);
			  });

		path = tree.getPathTo(root, 0);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(0, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Rght));
		ASSERT_EQUAL(10, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Rght));
		ASSERT_EQUAL(20, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Rght));
		ASSERT_EQUAL(Tree::IDX_END - 1, path->getIdx());
		ASSERT_TRUE(!path->moveToValid(Dir::Rght));

		path = tree.getPathTo(root, Tree::IDX_END - 1);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(path->moveToValid(Dir::Left));
		ASSERT_EQUAL(20, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Left));
		ASSERT_EQUAL(10, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Left));
		ASSERT_EQUAL(0, path->getIdx());
		ASSERT_TRUE(!path->moveToValid(Dir::Left));

		path = tree.getPathTo(root, 10);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(path->moveToValid(Dir::Rght, 0));
		ASSERT_EQUAL(20, path->getIdx());
		ASSERT_TRUE(path->moveToValid(Dir::Left, 0));
		ASSERT_EQUAL(10, path->getIdx());

		path = tree.getPathTo(root, 10);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(path->moveToValid(Dir::Rght, 1));
		ASSERT_EQUAL(Tree::IDX_END - 1, path->getIdx());
		ASSERT_TRUE(!path->moveToValid(Dir::Rght, 1));

		path = tree.getPathTo(root, 0);
		ASSERT_TRUE(path.has_value());
		ASSERT_TRUE(path->moveToValid(Dir::Rght, 2));
		ASSERT_EQUAL(Tree::IDX_END - 1, path->getIdx());
		ASSERT_TRUE(!path->moveToValid(Dir::Left, 4));

		path = tree.getPathTo(root, 15, Dir::Left);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(10, path->getIdx());
		path = tree.getPathTo(root, 15, Dir::Rght);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(20, path->getIdx());
		path = tree.getPathTo(root, 0, Dir::Left);
		ASSERT_TRUE(path.has_value());
		ASSERT_EQUAL(0, path->getIdx());
		path = tree.getPathTo(root, Tree::IDX_END, Dir::Rght);
		ASSERT_TRUE(!path.has_value());
	}

	void testSegmentTreeReadOnlyRangeTraversal() {
		using Tree = vm::persistent::detail::SegmentTree;

		Tree       tree;
		const auto leaf            = tree.emplaceLeaf(10, 100);
		usize      in_range_leaves = 0;

		tree.rebuildRange<void>(
			leaf,
			0,
			5,
			Tree::RangeBuilder<void>{
				.in_range     = [&](auto node, auto) { in_range_leaves += tree.getSize(node); },
				.out_of_range = [](auto, auto) {},
			}
		);
		tree.rebuildRange<void>(
			leaf,
			10,
			11,
			Tree::RangeBuilder<void>{
				.in_range     = [&](auto node, auto) { in_range_leaves += tree.getSize(node); },
				.out_of_range = [](auto, auto) {},
			}
		);

		ASSERT_EQUAL(1, in_range_leaves);
	}

	void testVector() {
		using namespace vm::persistent;

		Vector<std::string> vec;
		auto checker = [&](VectorStateID state, std::vector<std::string> expected) -> void {
			auto size = expected.size();
			ASSERT_EQUAL(expected.size(), vec.size(state));
			auto view = vec.view(state, 0, size);
			ASSERT_EQUAL(expected, view);

			VectorStateView state_view(vec, state);
			ASSERT_EQUAL(expected.size(), state_view.size());
			std::vector<std::string> iterated;
			for (const auto& item: state_view) iterated.push_back(item);
			ASSERT_EQUAL(expected, iterated);

			for (usize i = 0; i < size; i++) {
				ASSERT_EQUAL(expected[i], vec.at(state, i));
				ASSERT_EQUAL(expected[i], state_view[i]);
			}
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
		ASSERT_EQUAL(op15, vec.getPrefix(op15, vec.size(op15)));
		ASSERT_EQUAL(empt, vec.getPrefix(op15, 0));

		ASSERT_EQUAL(op16, op03);
		ASSERT_EQUAL(op02, op04);
		ASSERT_EQUAL(op05, op04);

		auto subview = vec.view(op07, 1, 4);
		ASSERT_EQUAL((std::vector<std::string>{ "val02", "val03", "val05" }), subview);

		auto empty_view = vec.view(op07, 2, 2);
		ASSERT_TRUE(empty_view.empty());

		auto op17 = vec.pop(op01);
		checker(op17, {});
		ASSERT_EQUAL(op17, empt);

		auto                     bulk_state = empt;
		std::vector<std::string> bulk_expected;
		for (usize i = 0; i < 20; i++) {
			auto s = "bulk_" + std::to_string(i);
			bulk_expected.push_back(s);
			bulk_state = vec.push(bulk_state, s);
		}
		checker(bulk_state, bulk_expected);

		for (usize i = 0; i < 10; i++) {
			bulk_expected.pop_back();
			bulk_state = vec.pop(bulk_state);
		}
		checker(bulk_state, bulk_expected);
	}

	void testRandomVector() {
		using namespace vm::persistent;

		DummyVector<std::string>   dummy;
		Vector<std::string>        real;
		std::vector<usize>         dummy_states = { DummyVector<std::string>::EMPTY };
		std::vector<VectorStateID> real_states  = { Vector<std::string>::EMPTY };
		std::mt19937_64            random{ 0x5E'ED'12'34 };

		auto check = [&](usize dummy_state, VectorStateID real_state) {
			ASSERT_EQUAL(dummy.size(dummy_state), real.size(real_state));
			for (usize i = 0; i < dummy.size(dummy_state); i++)
				ASSERT_EQUAL(dummy.at(dummy_state, i), real.at(real_state, i));
		};

		for (usize i = 0; i < 10'000; i++) {
			auto state_idx  = random() % real_states.size();
			auto dummy_id   = dummy_states.at(state_idx);
			auto real_state = real_states.at(state_idx);
			auto size       = dummy.size(dummy_id);
			auto operation  = random() % 3;

			if (operation == 0 || size == 0) {
				auto value = "random_" + std::to_string(i);
				dummy_states.push_back(dummy.push(dummy_id, value));
				real_states.push_back(real.push(real_state, value));
			} else if (operation == 1) {
				auto idx   = random() % size;
				auto value = "random_" + std::to_string(i);
				dummy_states.push_back(dummy.change(dummy_id, idx, value));
				real_states.push_back(real.change(real_state, idx, value));
			} else {
				auto amount = 1 + random() % size;
				dummy_states.push_back(dummy.pop(dummy_id, amount));
				real_states.push_back(real.pop(real_state, amount));
			}

			check(dummy_states.back(), real_states.back());
			if (i % 256 == 0) check(dummy_id, real_state);
		}
	}

	void testHashMap() {
		using namespace vm::persistent;

		HashMap<std::string, std::string> map;
		auto                              checker
			= [&](HashMapStateID state, base::HashMap<std::string, std::string> expected) -> void {
			ASSERT_EQUAL(expected.size(), map.size(state));
			auto map_copy = map.toMap(state);

			HashMapStateView state_view(map, state);
			ASSERT_EQUAL(expected.size(), state_view.size());
			base::HashMap<std::string, std::string> iterated;
			for (const auto& [k, v]: state_view) iterated.emplace(k, v);
			ASSERT_EQUAL(expected.size(), iterated.size());

			for (auto& [key, val]: expected) {
				CORE_ASSERT(
					map_copy.contains(key) && map.contains(state, key) && state_view.contains(key),
					"map should contain all of expected values"
				);
				CORE_ASSERT(
					map_copy.at(key) == val, "values should be equal in both copy and database"
				);

				CORE_ASSERT(
					map.at(state, key) == val && state_view.at(key) == val,
					"values should be equal in both copy and database"
				);

				CORE_ASSERT(
					iterated.contains(key) && iterated.at(key) == val,
					"iterated map should contain the key/value"
				);
			}

			for (auto& [key, val]: map_copy) {
				CORE_ASSERT(
					expected.contains(key) && map.contains(state, key) && state_view.contains(key),
					"map should contain all of expected values"
				);
				CORE_ASSERT(
					expected.at(key) == val && map.at(state, key) == val
						&& state_view.at(key) == val,
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

		auto op07 = map.insert(op06, "key4", "val4");
		checker(
			op07, { { "key1", "val1" }, { "key2", "val5" }, { "key3", "val2" }, { "key4", "val4" } }
		);

		auto op08 = map.erase(op07, "key1");
		auto op09 = map.erase(op08, "key2");
		auto op10 = map.erase(op09, "key3");
		auto op11 = map.erase(op10, "key4");
		checker(op11, {});
		ASSERT_EQUAL(op11, empt);

		auto                                    bulk_map = empt;
		base::HashMap<std::string, std::string> bulk_expected{};
		for (usize i = 0; i < 20; i++) {
			auto k = "k_" + std::to_string(i);
			auto v = "v_" + std::to_string(i);
			bulk_expected.emplace(k, v);
			bulk_map = map.insert(bulk_map, k, v);
		}
		checker(bulk_map, bulk_expected);
	}

	void testRandomHashMap() {
		using namespace vm::persistent;

		DummyHashMap<std::string, std::string> dummy;
		HashMap<std::string, std::string>      real;
		std::vector<usize> dummy_states         = { DummyHashMap<std::string, std::string>::EMPTY };
		std::vector<HashMapStateID> real_states = { HashMap<std::string, std::string>::EMPTY };
		std::mt19937_64             random{ 0xBA'DC'0F'FE };

		auto check = [&](usize dummy_state, HashMapStateID real_state, usize max_key) {
			for (usize i = 0; i <= max_key; i++) {
				auto key = "random_key_" + std::to_string(i);
				ASSERT_EQUAL(dummy.contains(dummy_state, key), real.contains(real_state, key));
				if (dummy.contains(dummy_state, key))
					ASSERT_EQUAL(dummy.at(dummy_state, key), real.at(real_state, key));
			}
		};

		for (usize i = 0; i < 1'000; i++) {
			auto state_idx  = random() % real_states.size();
			auto dummy_id   = dummy_states.at(state_idx);
			auto real_state = real_states.at(state_idx);
			auto key        = "random_key_" + std::to_string(i);
			auto value      = "random_value_" + std::to_string(random());

			dummy_states.push_back(dummy.insert(dummy_id, key, value));
			real_states.push_back(real.insert(real_state, key, value));

			check(dummy_states.back(), real_states.back(), i);
			if (i % 256 == 0) check(dummy_id, real_state, i);
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/utils/");

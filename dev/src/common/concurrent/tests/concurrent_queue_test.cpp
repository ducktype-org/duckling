// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <concurrent/base/collections/queue.hpp>

#include <tester/tester.hpp>

#include <atomic>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class ConcurrentQueueTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcurrentQueueTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(singleThreadedBasicOps);
		TESTER_ADD_TEST(singleThreadedIterateAndErase);
		TESTER_ADD_TEST(multiThreadedPushPopLossless2p3c);
		TESTER_ADD_TEST(multiThreadedPushPopLossless4p4c);
		TESTER_ADD_TEST(multiThreadedTryPopIfAndExtractIf);
		TESTER_ADD_TEST(multiThreadedRandomMixedOps2w);
		TESTER_ADD_TEST(multiThreadedRandomMixedOps4w);
	}

private:
	void singleThreadedBasicOps() {
		concurrent::ConQueue<int> queue;

		ASSERT_TRUE(queue.empty());
		ASSERT_EQUAL(queue.size(), 0ULL);

		queue.push(1);
		queue.push(2);
		queue.push(3);
		queue.push(4);

		ASSERT_TRUE(!queue.empty());
		ASSERT_EQUAL(queue.size(), 4ULL);

		auto miss_on_odd_front
			= queue.tryPopIf([](base::CRef<int> value) { return *value % 2 == 0; });
		ASSERT_TRUE(miss_on_odd_front.empty());
		ASSERT_EQUAL(queue.size(), 4ULL);

		auto first = queue.tryPop();
		ASSERT_HAS_VALUE(first);
		ASSERT_EQUAL(first.value(), 1);
		ASSERT_EQUAL(queue.size(), 3ULL);

		auto even = queue.tryPopIf([](base::CRef<int> value) { return *value % 2 == 0; });
		ASSERT_HAS_VALUE(even);
		ASSERT_EQUAL(even.value(), 2);
		ASSERT_EQUAL(queue.size(), 2ULL);

		auto extracted = queue.extractIf([](base::CRef<int> value) { return *value == 4; });
		ASSERT_HAS_VALUE(extracted);
		ASSERT_EQUAL(extracted.value(), 4);
		ASSERT_EQUAL(queue.size(), 1ULL);

		auto not_found = queue.extractIf([](base::CRef<int> value) { return *value == 42; });
		ASSERT_TRUE(not_found.empty());
		ASSERT_EQUAL(queue.size(), 1ULL);

		auto last = queue.tryPop();
		ASSERT_HAS_VALUE(last);
		ASSERT_EQUAL(last.value(), 3);

		ASSERT_TRUE(queue.tryPop().empty());
		ASSERT_TRUE(queue.empty());
		ASSERT_EQUAL(queue.size(), 0ULL);
	}

	void singleThreadedIterateAndErase() {
		concurrent::ConQueue<int> queue;
		for (int i = 0; i < 8; ++i) queue.push(i);

		std::vector<int> seen;
		for (const int value: queue) seen.push_back(value);
		ASSERT_EQUAL(seen.size(), 8ULL);
		for (int i = 0; i < 8; ++i) ASSERT_EQUAL(seen[static_cast<usize>(i)], i);

		std::vector<int> iterated;
		for (const int value: queue) iterated.push_back(value);

		ASSERT_EQUAL(queue.size(), 8ULL);
		ASSERT_EQUAL(iterated.size(), 8ULL);
		for (int i = 0; i < 8; ++i) ASSERT_EQUAL(iterated[static_cast<usize>(i)], i);
	}

	template<usize PRODUCER_COUNT, usize CONSUMER_COUNT>
	void multiThreadedPushPopLossless() {
		constexpr usize ITEMS_PER_PRODUCER = 3'000;
		constexpr usize TOTAL_ITEMS        = PRODUCER_COUNT * ITEMS_PER_PRODUCER;

		concurrent::ConQueue<u64> queue;
		std::atomic<usize>        consumed         = 0;
		std::atomic<bool>         has_out_of_range = false;
		std::atomic<bool>         has_duplicate    = false;
		std::vector<uint8_t>      seen(TOTAL_ITEMS, 0);
		std::mutex                seen_mutex;

		std::vector<std::jthread> producers;
		producers.reserve(PRODUCER_COUNT);
		for (usize producer_id = 0; producer_id < PRODUCER_COUNT; ++producer_id) {
			producers.emplace_back([&queue, producer_id]() {
				for (usize local_idx = 0; local_idx < ITEMS_PER_PRODUCER; ++local_idx) {
					auto value = static_cast<u64>(producer_id * ITEMS_PER_PRODUCER + local_idx);
					queue.push(value);
				}
			});
		}

		std::vector<std::jthread> consumers;
		consumers.reserve(CONSUMER_COUNT);
		for (usize consumer_id = 0; consumer_id < CONSUMER_COUNT; ++consumer_id) {
			consumers.emplace_back([&]() {
				while (consumed.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
					auto popped = queue.tryPop();
					if (!popped.has_value()) {
						std::this_thread::yield();
						continue;
					}

					auto idx = static_cast<usize>(popped.value());
					if (idx >= TOTAL_ITEMS) {
						has_out_of_range.store(true, std::memory_order_relaxed);
						continue;
					}

					{
						std::scoped_lock lock(seen_mutex);
						if (seen[idx] != 0) has_duplicate.store(true, std::memory_order_relaxed);
						seen[idx] = 1;
					}

					consumed.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		for (auto& producer: producers) producer.join();
		for (auto& consumer: consumers) consumer.join();

		ASSERT_EQUAL(consumed.load(), TOTAL_ITEMS);
		ASSERT_TRUE(queue.empty());
		ASSERT_EQUAL(queue.size(), 0ULL);
		ASSERT_TRUE(!has_out_of_range.load());
		ASSERT_TRUE(!has_duplicate.load());

		for (usize i = 0; i < TOTAL_ITEMS; ++i) ASSERT_TRUE(seen[i] == uint8_t(1));
	}

	void multiThreadedPushPopLossless2p3c() { multiThreadedPushPopLossless<2, 3>(); }

	void multiThreadedPushPopLossless4p4c() { multiThreadedPushPopLossless<4, 4>(); }

	void multiThreadedTryPopIfAndExtractIf() {
		constexpr usize TOTAL_ITEMS = 4'000;

		concurrent::ConQueue<u64> queue;
		for (usize i = 0; i < TOTAL_ITEMS; ++i) queue.push(static_cast<u64>(i));

		std::atomic<usize> evens_taken      = 0;
		std::atomic<usize> odds_taken       = 0;
		std::atomic<bool>  has_out_of_range = false;
		std::atomic<bool>  has_duplicate    = false;

		std::vector<uint8_t> seen(TOTAL_ITEMS, 0);
		std::mutex           seen_mutex;

		auto mark_seen = [&](u64 value) {
			auto idx = static_cast<usize>(value);
			if (idx >= TOTAL_ITEMS) {
				has_out_of_range.store(true, std::memory_order_relaxed);
				return;
			}
			std::scoped_lock lock(seen_mutex);
			if (seen[idx] != 0) has_duplicate.store(true, std::memory_order_relaxed);
			seen[idx] = 1;
		};

		std::jthread even_worker([&]() {
			while (evens_taken.load(std::memory_order_relaxed) < TOTAL_ITEMS / 2) {
				auto popped
					= queue.tryPopIf([](base::CRef<u64> value) { return (*value % 2) == 0; });
				if (!popped.has_value()) {
					std::this_thread::yield();
					continue;
				}

				auto value = popped.value();
				mark_seen(value);
				evens_taken.fetch_add(1, std::memory_order_relaxed);
			}
		});

		std::jthread odd_worker([&]() {
			while (odds_taken.load(std::memory_order_relaxed) < TOTAL_ITEMS / 2) {
				auto extracted
					= queue.extractIf([](base::CRef<u64> value) { return (*value % 2) == 1; });
				if (!extracted.has_value()) {
					std::this_thread::yield();
					continue;
				}

				auto value = extracted.value();
				mark_seen(value);
				odds_taken.fetch_add(1, std::memory_order_relaxed);
			}
		});

		even_worker.join();
		odd_worker.join();

		ASSERT_EQUAL(evens_taken.load(), TOTAL_ITEMS / 2);
		ASSERT_EQUAL(odds_taken.load(), TOTAL_ITEMS / 2);
		ASSERT_TRUE(queue.empty());
		ASSERT_EQUAL(queue.size(), 0ULL);
		ASSERT_TRUE(!has_out_of_range.load());
		ASSERT_TRUE(!has_duplicate.load());

		for (usize i = 0; i < TOTAL_ITEMS; ++i) ASSERT_TRUE(seen[i] == uint8_t(1));
	}

	template<usize WORKER_COUNT>
	void multiThreadedRandomMixedOps() {
		constexpr usize OPS_PER_WORKER = 2'000;
		constexpr usize TOTAL_OPS      = WORKER_COUNT * OPS_PER_WORKER;
		constexpr u64   RNG_SEED_BASE  = 0xC0'FF'EE'12'34ULL;

		concurrent::ConQueue<u64> queue;

		std::atomic<usize> push_ops    = 0;
		std::atomic<usize> pop_ops     = 0;
		std::atomic<usize> pop_if_ops  = 0;
		std::atomic<usize> extract_ops = 0;
		std::atomic<usize> iter_ops    = 0;

		std::atomic<usize>     performed_ops = 0;
		std::atomic<long long> net_delta     = 0;

		std::vector<std::jthread> workers;
		workers.reserve(WORKER_COUNT);

		for (usize worker_id = 0; worker_id < WORKER_COUNT; ++worker_id) {
			workers.emplace_back([&, worker_id]() {
				std::mt19937_64 rng(RNG_SEED_BASE + worker_id * 0x9E'37'79'B1'85'EB'CA'87ULL);
				std::uniform_int_distribution<int> op_dist(0, 4);
				std::uniform_int_distribution<u64> val_dist(0, 100'000);

				for (usize op_i = 0; op_i < OPS_PER_WORKER; ++op_i) {
					int op = op_dist(rng);

					switch (op) {
					case 0: {
						push_ops.fetch_add(1, std::memory_order_relaxed);
						u64 value = val_dist(rng);
						queue.push(value);
						net_delta.fetch_add(1, std::memory_order_relaxed);
						break;
					}
					case 1: {
						pop_ops.fetch_add(1, std::memory_order_relaxed);
						auto popped = queue.tryPop();
						if (popped.has_value()) net_delta.fetch_sub(1, std::memory_order_relaxed);
						break;
					}
					case 2: {
						pop_if_ops.fetch_add(1, std::memory_order_relaxed);
						u64  parity = val_dist(rng) & 1ULL;
						auto popped = queue.tryPopIf([parity](base::CRef<u64> value) {
							return ((*value) & 1ULL) == parity;
						});
						if (popped.has_value()) net_delta.fetch_sub(1, std::memory_order_relaxed);
						break;
					}
					case 3: {
						extract_ops.fetch_add(1, std::memory_order_relaxed);
						u64  mod_target = val_dist(rng) % 8ULL;
						auto extracted  = queue.extractIf([mod_target](base::CRef<u64> value) {
                            return (*value % 8ULL) == mod_target;
                        });
						if (extracted.has_value())
							net_delta.fetch_sub(1, std::memory_order_relaxed);
						break;
					}
					case 4: {
						iter_ops.fetch_add(1, std::memory_order_relaxed);
						// Iterate to exercise concurrent iteration.
						for ([[maybe_unused]] const auto& elem: queue) {}
						break;
					}
					default:
						CORE_PANIC("Unexpected randomized operation id");
					}

					performed_ops.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		for (auto& worker: workers) worker.join();

		ASSERT_EQUAL(performed_ops.load(std::memory_order_relaxed), TOTAL_OPS);

		auto ops_sum = push_ops.load(std::memory_order_relaxed)
		             + pop_ops.load(std::memory_order_relaxed)
		             + pop_if_ops.load(std::memory_order_relaxed)
		             + extract_ops.load(std::memory_order_relaxed)
		             + iter_ops.load(std::memory_order_relaxed);
		ASSERT_EQUAL(ops_sum, TOTAL_OPS);

		auto expected_size = net_delta.load(std::memory_order_relaxed);
		ASSERT_TRUE(expected_size >= 0);
		ASSERT_EQUAL(queue.size(), static_cast<usize>(expected_size));

		auto drained_count = 0ULL;
		while (queue.tryPop().has_value()) drained_count++;

		ASSERT_EQUAL(drained_count, static_cast<usize>(expected_size));
		ASSERT_TRUE(queue.empty());
		ASSERT_EQUAL(queue.size(), 0ULL);
	}

	void multiThreadedRandomMixedOps2w() { multiThreadedRandomMixedOps<2>(); }

	void multiThreadedRandomMixedOps4w() { multiThreadedRandomMixedOps<4>(); }
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");

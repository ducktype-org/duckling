#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/thread_id.hpp>
#include <vm/core/process/concurrency/fast_track/epoch.hpp>
#include <vm/core/process/concurrency/fast_track/fast_track_thread_data.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/process/concurrency/fast_track/vc.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

#include <string>
#include <utility>

using vm::Epoch;
using vm::FastTrackThreadData;
using vm::ShadowEntry;
using vm::VectorClock;
using vm::api::ThreadID;
using vm::exceptions::VMDataRaceException;
using vm::exceptions::VMFastTrackLimitException;

namespace {
	constexpr ThreadID T0{ 0 };
	constexpr ThreadID T1{ 1 };
	constexpr ThreadID T2{ 2 };

	/**
	 * @brief A thread as the engine sees it: its ID and its vector clock. `clock()` is the
	 * thread's current epoch component, what an access is stamped with.
	 */
	struct Thread {
		ThreadID    id;
		VectorClock vc;

		explicit Thread(ThreadID id): id(id) { vc[id] = 1; }

		[[nodiscard]] Epoch::Clock clock() const { return vc[id]; }

		[[nodiscard]] Epoch epoch() const { return { id, clock() }; }

		/// Lock release: publish the clock and start a new epoch.
		void release(VectorClock& lock) {
			lock = vc;
			vc.increment(id);
		}

		/// Lock acquire: learn everything the releaser knew.
		void acquire(const VectorClock& lock) { vc |= lock; }
	};

	/**
	 * @brief Runs `access` and tells whether it reported a race. Anything else propagates.
	 */
	template<typename Access>
	bool races(Access&& access) {
		try {
			std::forward<Access>(access)();
			return false;
		} catch (const VMDataRaceException&) { return true; }
	}

	/**
	 * @brief Runs `operation` and tells whether it hit an engine limit. Anything else propagates.
	 */
	template<typename Operation>
	bool limitExceeded(Operation&& operation) {
		try {
			std::forward<Operation>(operation)();
			return false;
		} catch (const VMFastTrackLimitException&) { return true; }
	}

	void read(ShadowEntry& entry, const Thread& thread) {
		entry.processRead(thread.id, thread.clock(), thread.vc);
	}

	void write(ShadowEntry& entry, const Thread& thread) {
		entry.processWrite(thread.id, thread.clock(), thread.vc);
	}
}

class FastTrackEngineTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FastTrackEngineTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testVectorClockDefaultsToZero);
		TESTER_ADD_TEST(testVectorClockGrowsOnWrite);
		TESTER_ADD_TEST(testVectorClockJoinIsPointwiseMax);
		TESTER_ADD_TEST(testVectorClockJoinWithEpoch);
		TESTER_ADD_TEST(testVectorClockJoinWithInitialEpochIsNoOp);
		TESTER_ADD_TEST(testVectorClockPartialOrder);
		TESTER_ADD_TEST(testVectorClockEquality);
		TESTER_ADD_TEST(testVectorClockIncrement);
		TESTER_ADD_TEST(testVectorClockRefusesThreadIdOutOfRange);
		TESTER_ADD_TEST(testVectorClockRefusesClockOverflow);
		TESTER_ADD_TEST(testEpochInitialState);
		TESTER_ADD_TEST(testEpochHappensBeforeVectorClock);
		TESTER_ADD_TEST(testEpochPacksThreadIdAndClock);
		TESTER_ADD_TEST(testFreshEntryNeverRaces);
		TESTER_ADD_TEST(testSameThreadAccessesNeverRace);
		TESTER_ADD_TEST(testOrderedReadStaysExclusive);
		TESTER_ADD_TEST(testConcurrentReadsSwitchToSharedMode);
		TESTER_ADD_TEST(testWriteWriteRace);
		TESTER_ADD_TEST(testWriteReadRace);
		TESTER_ADD_TEST(testReadWriteRaceExclusive);
		TESTER_ADD_TEST(testReadWriteRaceShared);
		TESTER_ADD_TEST(testSynchronizedAccessesDoNotRace);
		TESTER_ADD_TEST(testEntryIsUpdatedBeforeRaceIsReported);
		TESTER_ADD_TEST(testWriteLeavesSharedMode);
		TESTER_ADD_TEST(testCopyDoesNotShareReadSet);
		TESTER_ADD_TEST(testResetForgetsHistory);
		TESTER_ADD_TEST(testRaceReportNamesBothEpochs);
		TESTER_ADD_TEST(testForkAndJoinOrderChildAndParent);
		TESTER_ADD_TEST(testReleaseAndAcquireOrderThreads);
		TESTER_ADD_TEST(testForkStartsPastRecycledThreadIdClock);
		TESTER_ADD_TEST(testShadowLayoutOfNestedDataTypes);
		TESTER_ADD_TEST(testShadowLayoutOfVariants);
	}

	// ---- VectorClock ----

	void testVectorClockDefaultsToZero() {
		const VectorClock vc;
		ASSERT_EQUAL(0U, vc.size());
		ASSERT_EQUAL(0U, vc[T0]);
		ASSERT_EQUAL(0U, vc[ThreadID{ 1'000 }]);
		ASSERT_EQUAL(0U, vc[ThreadID::bad()]);
	}

	void testVectorClockGrowsOnWrite() {
		VectorClock vc;
		vc[T2] = 7;
		ASSERT_EQUAL(3U, vc.size());
		ASSERT_EQUAL(0U, vc[T0]);
		ASSERT_EQUAL(0U, vc[T1]);
		ASSERT_EQUAL(7U, vc[T2]);
	}

	void testVectorClockJoinIsPointwiseMax() {
		VectorClock shorter;
		shorter[T0] = 5;
		VectorClock longer;
		longer[T0] = 2;
		longer[T2] = 4;

		shorter |= longer;
		ASSERT_EQUAL(5U, shorter[T0]);
		ASSERT_EQUAL(0U, shorter[T1]);
		ASSERT_EQUAL(4U, shorter[T2]);

		longer |= shorter;
		ASSERT_EQUAL(5U, longer[T0]);
		ASSERT_EQUAL(4U, longer[T2]);
	}

	void testVectorClockJoinWithEpoch() {
		VectorClock vc;
		vc[T1] = 3;
		vc |= Epoch(T1, 2);
		ASSERT_EQUAL(3U, vc[T1]);
		vc |= Epoch(T1, 9);
		ASSERT_EQUAL(9U, vc[T1]);
		vc |= Epoch(T2, 1);
		ASSERT_EQUAL(1U, vc[T2]);
	}

	void testVectorClockJoinWithInitialEpochIsNoOp() {
		VectorClock vc;
		vc[T0] = 1;
		vc |= Epoch();
		ASSERT_EQUAL(1U, vc.size());
		ASSERT_EQUAL(1U, vc[T0]);
	}

	void testVectorClockPartialOrder() {
		VectorClock lower;
		lower[T0] = 1;
		VectorClock upper;
		upper[T0] = 1;
		upper[T1] = 1;

		ASSERT_TRUE(lower <= lower);
		ASSERT_TRUE(lower <= upper);
		ASSERT_TRUE(!(upper <= lower));

		VectorClock incomparable;
		incomparable[T2] = 1;
		ASSERT_TRUE(!(incomparable <= upper));
		ASSERT_TRUE(!(upper <= incomparable));
	}

	void testVectorClockEquality() {
		VectorClock a;
		a[T0] = 1;
		VectorClock b;
		b[T0] = 1;
		b[T1] = 0;
		ASSERT_TRUE(a == b);
		b[T1] = 1;
		ASSERT_TRUE(!(a == b));
	}

	void testVectorClockIncrement() {
		VectorClock vc;
		vc.increment(T1);
		vc.increment(T1);
		ASSERT_EQUAL(2U, vc[T1]);
		ASSERT_EQUAL(0U, vc[T0]);
	}

	void testVectorClockRefusesThreadIdOutOfRange() {
		VectorClock    vc;
		const ThreadID past_the_last{ Epoch::MAX_TID + 1 };
		// Reading past the end is 0, whatever the ID.
		ASSERT_EQUAL(Epoch::Clock(0), std::as_const(vc)[ThreadID::bad()]);
		ASSERT_EQUAL(Epoch::Clock(0), std::as_const(vc)[past_the_last]);
		// Writing grows the clock, which only an ID an epoch can name may do.
		ASSERT_TRUE(limitExceeded([&] { vc[ThreadID::bad()] = 1; }));
		ASSERT_TRUE(limitExceeded([&] { vc[past_the_last] = 1; }));
		ASSERT_TRUE(limitExceeded([&] { vc.increment(ThreadID::bad()); }));
		ASSERT_EQUAL(usize(0), vc.size());
	}

	void testVectorClockRefusesClockOverflow() {
		VectorClock vc;
		vc[T0] = Epoch::MAX_CLOCK - 1;
		vc.increment(T0);
		ASSERT_EQUAL(Epoch::MAX_CLOCK, vc[T0]);
		ASSERT_TRUE(limitExceeded([&] { vc.increment(T0); }));
		ASSERT_EQUAL(Epoch::MAX_CLOCK, vc[T0]);
	}

	// ---- Epoch ----

	void testEpochInitialState() {
		const Epoch initial;
		ASSERT_TRUE(!initial.tid().isGood());
		ASSERT_EQUAL(0U, initial.clock());
		ASSERT_TRUE(initial == Epoch());
		ASSERT_TRUE(initial != Epoch(T0, 0));
	}

	void testEpochHappensBeforeVectorClock() {
		VectorClock vc;
		vc[T1] = 3;
		ASSERT_TRUE(Epoch(T1, 3) <= vc);
		ASSERT_TRUE(!(Epoch(T1, 4) <= vc));
		ASSERT_TRUE(!(Epoch(T2, 1) <= vc));
		ASSERT_TRUE(Epoch() <= vc);
		ASSERT_TRUE(Epoch() <= VectorClock());
	}

	void testEpochPacksThreadIdAndClock() {
		ASSERT_EQUAL(sizeof(u64), sizeof(Epoch));

		const Epoch epoch(T2, 7);
		ASSERT_TRUE(epoch.tid() == T2);
		ASSERT_EQUAL(Epoch::Clock(7), epoch.clock());

		const ThreadID last_tid{ Epoch::MAX_TID };
		const Epoch    largest(last_tid, Epoch::MAX_CLOCK);
		ASSERT_TRUE(largest.tid() == last_tid);
		ASSERT_EQUAL(Epoch::MAX_CLOCK, largest.clock());
		ASSERT_TRUE(largest != Epoch());

		ASSERT_TRUE(Epoch().tid().isBad());
		ASSERT_EQUAL(Epoch::Clock(0), Epoch().clock());
		ASSERT_TRUE(Epoch(ThreadID::bad(), 0) == Epoch());

#if defined(BUILD_TYPE_DEV)
		// The good ID that would pack as the bad one is refused, not silently turned into it.
		assertThrows<base::Panic>(
			[] { (void) Epoch(ThreadID{ Epoch::MAX_TID + 1 }, 0); },
			"An ID past MAX_TID was packed into an epoch"
		);
#endif
	}

	// ---- ShadowEntry ----

	void testFreshEntryNeverRaces() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		ASSERT_TRUE(!races([&] { read(entry, t0); }));
		ASSERT_TRUE(!races([&] { read(entry, t1); }));

		ShadowEntry written;
		ASSERT_TRUE(!races([&] { write(written, t1); }));
		ASSERT_TRUE(written.last_write == t1.epoch());
	}

	void testSameThreadAccessesNeverRace() {
		ShadowEntry entry;
		Thread      t0(T0);
		ASSERT_TRUE(!races([&] { write(entry, t0); }));
		ASSERT_TRUE(!races([&] { read(entry, t0); }));
		ASSERT_TRUE(!races([&] { write(entry, t0); }));
		t0.vc.increment(T0);
		ASSERT_TRUE(!races([&] { read(entry, t0); }));
		ASSERT_TRUE(!races([&] { write(entry, t0); }));
		ASSERT_TRUE(!entry.isShared());
	}

	void testOrderedReadStaysExclusive() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		VectorClock lock;

		read(entry, t0);
		t0.release(lock);
		t1.acquire(lock);
		ASSERT_TRUE(!races([&] { read(entry, t1); }));
		ASSERT_TRUE(!entry.isShared());
		ASSERT_TRUE(entry.last_read_epoch == t1.epoch());
	}

	void testConcurrentReadsSwitchToSharedMode() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);

		read(entry, t0);
		ASSERT_TRUE(!races([&] { read(entry, t1); }));
		ASSERT_TRUE(entry.isShared());
		ASSERT_EQUAL(t0.clock(), (*entry.last_read_vc)[T0]);
		ASSERT_EQUAL(t1.clock(), (*entry.last_read_vc)[T1]);
	}

	void testWriteWriteRace() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		write(entry, t0);
		ASSERT_TRUE(races([&] { write(entry, t1); }));
	}

	void testWriteReadRace() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		write(entry, t0);
		ASSERT_TRUE(races([&] { read(entry, t1); }));
	}

	void testReadWriteRaceExclusive() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		read(entry, t0);
		ASSERT_TRUE(!entry.isShared());
		ASSERT_TRUE(races([&] { write(entry, t1); }));
	}

	void testReadWriteRaceShared() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		Thread      t2(T2);
		VectorClock lock;

		read(entry, t0);
		read(entry, t1);
		ASSERT_TRUE(entry.isShared());

		// t2 is ordered after t1's read only, so it still races with t0's.
		t1.release(lock);
		t2.acquire(lock);
		ASSERT_TRUE(races([&] { write(entry, t2); }));
	}

	void testSynchronizedAccessesDoNotRace() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		VectorClock lock;

		write(entry, t0);
		t0.release(lock);
		t1.acquire(lock);
		ASSERT_TRUE(!races([&] { read(entry, t1); }));
		ASSERT_TRUE(!races([&] { write(entry, t1); }));

		t1.release(lock);
		t0.acquire(lock);
		ASSERT_TRUE(!races([&] { write(entry, t0); }));
	}

	void testEntryIsUpdatedBeforeRaceIsReported() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);

		write(entry, t0);
		ASSERT_TRUE(races([&] { write(entry, t1); }));
		ASSERT_TRUE(entry.last_write == t1.epoch());
		ASSERT_TRUE(entry.last_read_epoch == t1.epoch());
		// The racing thread carries on: its own later accesses are ordered after its write.
		ASSERT_TRUE(!races([&] { read(entry, t1); }));
		ASSERT_TRUE(!races([&] { write(entry, t1); }));

		ShadowEntry shared;
		read(shared, t0);
		read(shared, t1);
		ASSERT_TRUE(shared.isShared());
		ASSERT_TRUE(races([&] { write(shared, t1); }));
		ASSERT_TRUE(!shared.isShared());
		ASSERT_TRUE(shared.last_write == t1.epoch());
	}

	void testWriteLeavesSharedMode() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		VectorClock lock;

		read(entry, t0);
		read(entry, t1);
		ASSERT_TRUE(entry.isShared());

		t0.release(lock);
		t1.acquire(lock);
		ASSERT_TRUE(!races([&] { write(entry, t1); }));
		ASSERT_TRUE(!entry.isShared());
		ASSERT_TRUE(entry.last_write == t1.epoch());
	}

	void testCopyDoesNotShareReadSet() {
		ShadowEntry original;
		Thread      t0(T0);
		Thread      t1(T1);
		Thread      t2(T2);
		read(original, t0);
		read(original, t1);
		ASSERT_TRUE(original.isShared());

		ShadowEntry copy(original);
		ASSERT_TRUE(copy.isShared());
		ASSERT_TRUE(copy.last_read_vc.get() != original.last_read_vc.get());

		read(copy, t2);
		ASSERT_EQUAL(t2.clock(), (*copy.last_read_vc)[T2]);
		ASSERT_EQUAL(0U, (*original.last_read_vc)[T2]);

		ShadowEntry assigned;
		assigned = original;
		ASSERT_TRUE(assigned.last_read_vc.get() != original.last_read_vc.get());
		ASSERT_TRUE(*assigned.last_read_vc == *original.last_read_vc);

		ShadowEntry moved(std::move(assigned));
		ASSERT_TRUE(moved.isShared());
		ASSERT_TRUE(*moved.last_read_vc == *original.last_read_vc);
	}

	void testResetForgetsHistory() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		read(entry, t0);
		read(entry, t1);
		ASSERT_TRUE(races([&] { write(entry, t1); }));

		entry.reset();
		ASSERT_TRUE(entry.last_write == Epoch());
		ASSERT_TRUE(entry.last_read_epoch == Epoch());
		ASSERT_TRUE(!entry.isShared());
		ASSERT_TRUE(!races([&] { write(entry, t0); }));
	}

	// ---- Thread data: fork, join, acquire, release ----

	/// A write by `thread` to `entry`, stamped with the thread's current epoch.
	static void writeBy(ShadowEntry& entry, const FastTrackThreadData& thread) {
		entry.processWrite(thread.thread_id, thread.getCurrentEpoch().clock(), thread.getVC());
	}

	void testForkAndJoinOrderChildAndParent() {
		FastTrackThreadData main;
		main.forkVC(VectorClock(), T0);  // The first thread starts from an empty clock.
		ASSERT_EQUAL(Epoch::Clock(1), main.getCurrentEpoch().clock());

		ShadowEntry before_fork;
		writeBy(before_fork, main);
		FastTrackThreadData child;
		child.forkVC(main.getVC(), T1);
		ASSERT_TRUE(!races([&] { writeBy(before_fork, child); }));  // ordered by the fork

		ShadowEntry by_child;
		writeBy(by_child, child);
		ASSERT_TRUE(races([&] { writeBy(by_child, main); }));  // main has not joined yet

		main.joinVC(child.getVC());
		ShadowEntry by_child_too;
		writeBy(by_child_too, child);
		ASSERT_TRUE(!races([&] { writeBy(by_child_too, main); }));  // ordered by the join
	}

	void testReleaseAndAcquireOrderThreads() {
		FastTrackThreadData t0;
		FastTrackThreadData t1;
		t0.forkVC(VectorClock(), T0);
		t1.forkVC(VectorClock(), T1);
		VectorClock lock;

		ShadowEntry entry;
		writeBy(entry, t0);
		const Epoch::Clock before_release = t0.getCurrentEpoch().clock();
		t0.onRelease(lock);
		ASSERT_EQUAL(before_release + 1, t0.getCurrentEpoch().clock());  // a new epoch
		t1.onAcquire(lock);
		ASSERT_TRUE(!races([&] { writeBy(entry, t1); }));                // ordered by the lock

		ShadowEntry unlocked;
		writeBy(unlocked, t1);
		ASSERT_TRUE(races([&] { writeBy(unlocked, t0); }));  // t0 learned nothing from t1
	}

	/**
	 * Thread IDs are pool slots: after "spawn, spawn again" the second child gets the first
	 * one's ID and the thread data of that slot. It has to start past every clock the first
	 * child handed out, or its accesses look ordered before what a third thread learned from
	 * the first child through a lock. The parent never learns the first child's clock here, so
	 * only the slot's own last clock can push the second child past it.
	 */
	void testForkStartsPastRecycledThreadIdClock() {
		FastTrackThreadData main;
		main.forkVC(VectorClock(), T0);
		FastTrackThreadData other;
		other.forkVC(VectorClock(), T2);
		VectorClock lock;

		FastTrackThreadData slot;  // reused for both children, as the thread pool does
		slot.forkVC(main.getVC(), T1);
		for (int i = 0; i < 3; ++i) slot.onRelease(lock);  // the first child reaches 4@t1
		other.onAcquire(lock);                             // `other` knows t1 up to 3
		const Epoch::Clock last_of_first_child = slot.getCurrentEpoch().clock();
		ASSERT_EQUAL(Epoch::Clock(4), last_of_first_child);
		ASSERT_EQUAL(Epoch::Clock(0), main.getVC()[T1]);  // main learned nothing of it

		slot.forkVC(main.getVC(), T1);
		ASSERT_TRUE(slot.getCurrentEpoch().clock() > last_of_first_child);

		// The second child's write is not ordered before `other`, which only knows the first
		// child's clocks: a race. A second child restarted at 1@t1 would hide it.
		ShadowEntry by_second_child;
		writeBy(by_second_child, slot);
		ASSERT_TRUE(races([&] { writeBy(by_second_child, other); }));
	}

	// ---- Type shadow layout ----

	void testShadowLayoutOfNestedDataTypes() {
		using vm::Type;
		Type byte_type = Type::declareType(base::StrID("u8"));
		byte_type.definePrimitive(Bytes(1), 1);
		Type word_type = Type::declareType(base::StrID("u64"));
		word_type.definePrimitive(Bytes(8), 1);

		// { u8 a; <7 bytes of padding>; u64 b; }: 16 bytes, 2 shadow entries.
		Type inner = Type::declareType(base::StrID("inner"));
		inner.defineData(
			{ { base::StrID("a"), &byte_type, Bytes(0), 0 },
		      { base::StrID("b"), &word_type, Bytes(8), 1 } },
			Bytes(16),
			{},
			2
		);
		// { u8 c; <7>; inner d; inner e[2]; }: 56 bytes, 1 + 2 + 4 shadow entries.
		Type table = Type::declareType(base::StrID("inner[2]"));
		table.defineFixedSizeTable(&inner, 2, 4);
		Type outer = Type::declareType(base::StrID("outer"));
		outer.defineData(
			{ { base::StrID("c"), &byte_type, Bytes(0), 0 },
		      { base::StrID("d"), &inner, Bytes(8), 1 },
		      { base::StrID("e"), &table, Bytes(24), 3 } },
			Bytes(56),
			{},
			7
		);
		outer.finalize();

		ASSERT_EQUAL(2U, inner.getShadowSize());
		ASSERT_EQUAL(0U, inner.getShadowEntryIndex(0));
		ASSERT_NO_VALUE(inner.shadowEntryIndexOrPadding(3));
		ASSERT_EQUAL(1U, inner.getShadowEntryIndex(8));
		ASSERT_EQUAL(1U, inner.getShadowEntryIndex(15));

		ASSERT_EQUAL(7U, outer.getShadowSize());
		ASSERT_EQUAL(0U, outer.getShadowEntryIndex(0));
		ASSERT_NO_VALUE(outer.shadowEntryIndexOrPadding(4));
		ASSERT_EQUAL(1U, outer.getShadowEntryIndex(8));
		ASSERT_NO_VALUE(outer.shadowEntryIndexOrPadding(12));
		ASSERT_EQUAL(2U, outer.getShadowEntryIndex(16));
		ASSERT_EQUAL(3U, outer.getShadowEntryIndex(24));
		ASSERT_EQUAL(4U, outer.getShadowEntryIndex(32));
		ASSERT_EQUAL(5U, outer.getShadowEntryIndex(40));
		ASSERT_NO_VALUE(outer.shadowEntryIndexOrPadding(41));
		ASSERT_EQUAL(6U, outer.getShadowEntryIndex(55));

		ASSERT_EQUAL(4U, table.getShadowSize());
		ASSERT_EQUAL(3U, table.getShadowEntryIndex(24));
	}

	void testShadowLayoutOfVariants() {
		using vm::Type;
		Type byte_type = Type::declareType(base::StrID("u8"));
		byte_type.definePrimitive(Bytes(1), 1);
		Type word_type = Type::declareType(base::StrID("u64"));
		word_type.definePrimitive(Bytes(8), 1);
		Type pair = Type::declareType(base::StrID("pair"));
		pair.defineData(
			{ { base::StrID("a"), &word_type, Bytes(0), 0 },
		      { base::StrID("b"), &word_type, Bytes(8), 1 } },
			Bytes(16),
			{},
			2
		);
		// variant { u8, pair }: a 1-byte tag and a 16-byte payload, 1 + 2 shadow entries.
		Type variant = Type::declareType(base::StrID("variant"));
		variant.defineVariant(Bytes(1), { &byte_type, &pair }, 3);
		variant.finalize();

		ASSERT_EQUAL(3U, variant.getShadowSize());
		ASSERT_EQUAL(0U, variant.getShadowEntryIndex(0));
		// Every payload byte maps to the first payload entry; the nested block of the active
		// alternative is the only way to entry 2.
		ASSERT_EQUAL(1U, variant.getShadowEntryIndex(1));
		ASSERT_EQUAL(1U, variant.getShadowEntryIndex(9));
		ASSERT_EQUAL(1U, variant.getShadowEntryIndex(16));
	}

	void testRaceReportNamesBothEpochs() {
		ShadowEntry entry;
		Thread      t0(T0);
		Thread      t1(T1);
		t0.vc.increment(T0);
		write(entry, t0);

		std::string message;
		try {
			write(entry, t1);
		} catch (const VMDataRaceException& e) { message = e.what(); }
		ASSERT_TRUE(message.contains("Write-Write"));
		ASSERT_TRUE(message.contains("2@t0"));
		ASSERT_TRUE(message.contains("1@t1"));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/fast_track/");

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/process_options.hpp>
#include <vm/core/safe/exceptions.hpp>

#include <string>

/**
 * @brief Fast Track end-to-end tests: every `*_race.dbc` program has to panic with a data race
 * report, every `*_no_race.dbc` program has to run to completion without one.
 */
class VmFastTrackTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFastTrackTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(fastTrackRaceTest);
		TESTER_ADD_TEST(fastTrackNoRaceTest);
		TESTER_ADD_TEST(fastTrackDisabledRaceTest);
		TESTER_ADD_TEST(minimalNoRaceTest);
		TESTER_ADD_TEST(extensiveNoRaceTest);
		TESTER_ADD_TEST(extensiveStackRaceTest);
		TESTER_ADD_TEST(extensiveHeapRaceTest);
		TESTER_ADD_TEST(block1ScalarRaceTest);
		TESTER_ADD_TEST(block1CmovRaceTest);
		TESTER_ADD_TEST(block1CmovNoRaceTest);
		TESTER_ADD_TEST(block2FreeRaceTest);
		TESTER_ADD_TEST(block3StructRaceTest);
		TESTER_ADD_TEST(block4ArrayRaceTest);
		TESTER_ADD_TEST(block5VariantRaceTest);
		TESTER_ADD_TEST(block6OpaqueRaceTest);
		TESTER_ADD_TEST(block7DyntableRaceTest);
		TESTER_ADD_TEST(block8SharedWriteRaceTest);
		TESTER_ADD_TEST(block8SharedNoRaceTest);
		TESTER_ADD_TEST(block9FreeReadRaceTest);
		TESTER_ADD_TEST(noRaceTest2);
		TESTER_ADD_TEST(block10CvNoRaceTest);
		TESTER_ADD_TEST(block11ExclusiveSharedNoRaceTest);
		TESTER_ADD_TEST(block12DyntableReallocRaceTest);
		TESTER_ADD_TEST(block13DyntableReallocNoRaceTest);
		TESTER_ADD_TEST(block14VirtualCallNoRaceTest);
		TESTER_ADD_TEST(block15CallThenStackRaceTest);
		TESTER_ADD_TEST(block15CallThenStackNoRaceTest);
		TESTER_ADD_TEST(block16StructDisjointFieldsNoRaceTest);
		TESTER_ADD_TEST(block16StructSecondFieldRaceTest);
		TESTER_ADD_TEST(block17FixedSizeTableOutOfBoundsTest);
	}

private:
	static constexpr vm::api::ProcessConfig FAST_TRACK{ .enable_fast_track = true };

	vm::PID loadWithFastTrack(const std::string& filename) {
		auto pid = initProcess(FAST_TRACK);
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path(filename)) }));
		return pid;
	}

	/**
	 * @brief The program has a data race: the process has to panic with a race report. A
	 * panicked process cannot be deinitialized, it gets killed.
	 */
	void runRaceTest(const std::string& filename) {
		auto pid = loadWithFastTrack(filename);
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(pid), vm::exceptions::VMDataRaceException::ERR_MSG
		);
	}

	/**
	 * @brief The program is race free: it has to run to completion and deinitialize cleanly.
	 */
	void runNoRaceTest(const std::string& filename) {
		auto pid = loadWithFastTrack(filename);
		runTestOnVm(pid);
	}

	void fastTrackRaceTest() { runRaceTest("race_test.dbc"); }

	void fastTrackNoRaceTest() { runNoRaceTest("no_race_test.dbc"); }

	/** The same racy program runs to completion when Fast Track is off. */
	void fastTrackDisabledRaceTest() {
		auto pid = initProcess({ .enable_fast_track = false });
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("race_test.dbc")) }));
		runTestOnVm(pid);
	}

	// ---------------------------------------------------------------
	// Extensive tests: heap/stack/global + nested structs + tables
	// ---------------------------------------------------------------

	void minimalNoRaceTest() { runNoRaceTest("minimal_no_race.dbc"); }

	/** All reads/writes happen AFTER join - FastTrack must NOT report a race. */
	void extensiveNoRaceTest() { runNoRaceTest("extensive_no_race.dbc"); }

	/** Main thread and child thread both write to the same stack variable via a
	 *  shared pointer without synchronisation - FastTrack MUST detect a race. */
	void extensiveStackRaceTest() { runRaceTest("extensive_stack_race.dbc"); }

	/** Main thread and child thread both write to the same nested heap struct
	 *  array element without synchronisation - FastTrack MUST detect a race. */
	void extensiveHeapRaceTest() { runRaceTest("extensive_heap_race.dbc"); }

	void block1ScalarRaceTest() { runRaceTest("block1_scalar_race.dbc"); }

	/** Two threads both cmov_p64_p64 / cmov_p64_imm to the same global with the
	 *  condition always true and no synchronisation - FastTrack MUST detect a race. */
	void block1CmovRaceTest() { runRaceTest("block1_cmov_race.dbc"); }

	/** Same cmov pattern but the child is joined before the main thread writes -
	 *  FastTrack must NOT produce a false-positive race report. */
	void block1CmovNoRaceTest() { runNoRaceTest("block1_cmov_no_race.dbc"); }

	void block2FreeRaceTest() { runRaceTest("block2_free_race.dbc"); }

	void block3StructRaceTest() { runRaceTest("block3_struct_race.dbc"); }

	void block4ArrayRaceTest() { runRaceTest("block4_array_race.dbc"); }

	void block5VariantRaceTest() { runRaceTest("block5_variant_race.dbc"); }

	void block6OpaqueRaceTest() { runRaceTest("block6_opaque_race.dbc"); }

	void block7DyntableRaceTest() { runRaceTest("block7_dyntable_race.dbc"); }

	void block8SharedWriteRaceTest() { runRaceTest("block8_shared_write_race.dbc"); }

	void block8SharedNoRaceTest() { runNoRaceTest("block8_shared_no_race.dbc"); }

	void block9FreeReadRaceTest() { runRaceTest("block9_free_read_race.dbc"); }

	void noRaceTest2() { runNoRaceTest("no_race_test_2.dbc"); }

	// CV happens-before: write outside lock, signal inside lock, read outside lock after wait.
	// Verifies no false-positive race when HB flows through mutex lock_vc (pitfall: broken
	// onAcquire).
	void block10CvNoRaceTest() { runNoRaceTest("block10_cv_no_race.dbc"); }

	// Exclusive-to-Shared shadow transition: two readers trigger E->S, writer after join must not
	// race. Verifies no false-positive after E->S transition (pitfall: stale epoch in Shared VC
	// construction).
	void block11ExclusiveSharedNoRaceTest() {
		runNoRaceTest("block11_exclusive_shared_no_race.dbc");
	}

	// DynTable realloc race: concurrent writes to a newly-added element must be detected.
	// Verifies ft_dynTableReAlloc properly tracks new elements (pitfall: missing shadow init).
	void block12DyntableReallocRaceTest() { runRaceTest("block12_dyntable_realloc_race.dbc"); }

	// DynTable realloc no-race: write after join+realloc must not produce a false positive.
	// Verifies that realloc preserves existing shadow HB (pitfall: realloc wiping old entries).
	void block13DyntableReallocNoRaceTest() {
		runNoRaceTest("block13_dyntable_realloc_no_race.dbc");
	}

	// A virtual call pushes the callee's shadow frame itself, as its callee is only known at run
	// time; the callee's `ft_ret` used to pop a frame nobody had pushed.
	void block14VirtualCallNoRaceTest() { runNoRaceTest("block14_virtual_call_no_race.dbc"); }

	// `ft_call_func` and `ft_ret` have to cancel out: after a call to a non-void function the
	// caller's shadow head must be back where the data side's slot stack is, or a race on a
	// local initialized after the call is missed (and a race-free run reports one).
	void block15CallThenStackRaceTest() { runRaceTest("block15_call_then_stack_race.dbc"); }

	void block15CallThenStackNoRaceTest() { runNoRaceTest("block15_call_then_stack_no_race.dbc"); }

	// A field access through a struct pointer is checked on the field's own shadow entry, so
	// writes to different fields do not race and writes to the same second field do.
	void block16StructDisjointFieldsNoRaceTest() {
		runNoRaceTest("block16_struct_disjoint_fields_no_race.dbc");
	}

	void block16StructSecondFieldRaceTest() { runRaceTest("block16_struct_second_field_race.dbc"); }

	// The shadow of a stack fixed-size table is only touched after the data op has
	// bounds-checked the index: an out-of-bounds store panics the usual way instead of writing
	// past the shadow stack.
	void block17FixedSizeTableOutOfBoundsTest() {
		auto pid = loadWithFastTrack("block17_fst_out_of_bounds.dbc");
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(pid), vm::exceptions::VMOutOfBlockBoundsException::ERR_MSG
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");

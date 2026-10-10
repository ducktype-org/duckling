// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

class VmCorrectnessTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCorrectnessTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testAckermannOld);
		TESTER_ADD_TEST(testAckermannNew);
		TESTER_ADD_TEST(testCollatz);
		TESTER_ADD_TEST(testFibIter);
		TESTER_ADD_TEST(testFibRec);
		TESTER_ADD_TEST(testTailCall);
	}

private:
	// Note: these tests treat 16f17da CG as a reference
	// TODO: add more inputs and some corner cases
	void testAckermannOld() { runTestOnVm("ackermann_old.dbc", "3 3", "61", {}); }

	void testAckermannNew() { runTestOnVm("ackermann_new.dbc", "3 3", "61", {}); }

	void testCollatz() { runTestOnVm("collatz.dbc", "4242", "1276936", {}); }

	void testFibIter() {
		runTestOnVm("../../common/programs/fib_iter.dbc", "1000000 10000", "6875", {});
	}

	void testFibRec() { runTestOnVm("../../common/programs/fib_rec.dbc", "28", "317811", {}); }

	void testTailCall() { runTestOnVm("tailcall.dbc", "1000000", "0", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/correctness/");

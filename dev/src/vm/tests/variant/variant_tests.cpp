// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <filesystem/file_path.hpp>
#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <limits>

class VmVariantTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verySimpleVariant);
		TESTER_ADD_TEST(simpleVariant0);
		TESTER_ADD_TEST(blocksDontDisappearTest);
		TESTER_ADD_TEST(nestedVariantTest);
		TESTER_ADD_TEST(nestedCopy);
		TESTER_ADD_TEST(variantInsideStruct);
	}

private:
	void verySimpleVariant() { runTestOnVm("very_simple_variant.dbc", "42", "42"); }

	void simpleVariant0() { runTestOnVm("../common/programs/simple_variant.dbc", "0", "13"); }

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }

	void nestedVariantTest() { runTestOnVm("nested.dbc", "15", "15"); }

	void nestedCopy() { runTestOnVm("nested_copy.dbc", "3", "3"); }

	void variantInsideStruct() { runTestOnVm("inside_struct.dbc"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/variant/");

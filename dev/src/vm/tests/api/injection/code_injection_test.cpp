// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/collections/optional.hpp>

#include <string>
#include <vector>

/**
 * @brief What belongs here: adding code to a process that already has code loaded, through
 * `vm::api`. That covers loading several files, injecting new functions and globals, and the
 * REPL-style flow built on top of it.
 *
 * Running one function of a loaded program on its own goes to `api/run_function/`.
 */
class VmCodeInjectionTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCodeInjectionTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleFiles);
		TESTER_ADD_TEST(injectCode);
		TESTER_ADD_TEST(repl);
		TESTER_ADD_TEST(replWithGlobals);
		TESTER_ADD_TEST(incrementalGlobalVariantPersistsValue);
		TESTER_ADD_TEST(separateGlobals);
		TESTER_ADD_TEST(cyclicRepl);
	}

private:
	void multipleFiles() {
		vm::PID  pid = initProcess();
		fs::File file1(path("multiple_files_1.dbc"));
		fs::File file2(path("multiple_files_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file1, file2 }), "loadFiles");

		runAndCheckReturnValue(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void injectCode() {
		vm::PID  pid = initProcess();
		fs::File file1(path("inject_code_1.dbc"));
		fs::File file2(path("inject_code_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file1 }), "Load of 'inject_code_1.dbc'");
		assertSucceeded(vm::api::loadFiles(pid, { file2 }), "Load of 'inject_code_2.dbc'");

		runAndCheckReturnValue(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::File file1(path("../../common/programs/repl_1.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file1 }), "Load of 'repl_1.dbc'");
		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments), {}, {}, 32);
		freeArguments(owned_arguments);

		fs::File file2(path("repl_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file2 }), "Load of 'repl_2.dbc'");
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 1, 2 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments2), {}, {}, 3);
		freeArguments(owned_arguments2);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void replWithGlobals() {
		vm::PID pid = spawnAndLoad("repl_with_globals_1.dbc");
		runAndCheckReturnValue(pid, "globaler_setter", vm::FunctionRunArguments{}, "1 2", {}, {});

		fs::File file2(path("repl_with_globals_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file2 }), "Load of 'repl_with_globals_2.dbc'");
		runAndCheckReturnValue(pid, "globaler_reader", vm::FunctionRunArguments{}, {}, "12", {});
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void incrementalGlobalVariantPersistsValue() {
		vm::PID pid = spawnAndLoad("incremental_global_variant_1.dbc");

		runAndCheckReturnValue(
			pid, "set_global_variant_value", vm::FunctionRunArguments{}, {}, {}, {}
		);

		fs::File file2(path("incremental_global_variant_2.dbc"));
		assertSucceeded(
			vm::api::loadFiles(pid, { file2 }), "Load of 'incremental_global_variant_2.dbc'"
		);

		runAndCheckReturnValue(
			pid, "read_global_variant_value", vm::FunctionRunArguments{}, {}, "735", {}
		);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void cyclicRepl() {
		vm::PID pid = initProcess();

		fs::File file1(path("loaded_func_call_1.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file1 }), "Load of 'loaded_func_call_1.dbc'");
		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, {}, 12);
		freeArguments(owned_arguments);

		fs::File file2(path("loaded_func_call_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file2 }), "Load of 'loaded_func_call_2.dbc'");
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 2, 3 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments2), {}, {}, 10);
		freeArguments(owned_arguments2);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void separateGlobals() {
		vm::PID pid = spawnAndLoad("separate_globals_1.dbc");

		fs::File file2(path("separate_globals_2.dbc"));
		assertSucceeded(vm::api::loadFiles(pid, { file2 }), "Load of 'separate_globals_2.dbc'");
		runAndCheckReturnValue(pid, "globaler_setter", vm::FunctionRunArguments{}, "12", "12", {});
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/injection/");

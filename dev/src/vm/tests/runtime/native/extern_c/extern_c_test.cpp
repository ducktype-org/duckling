// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/extc/extc_function.hpp>

#include <array>
#include <stdexcept>
#include <tuple>
#include <variant>

namespace simple {
	i64 add(i64 a, i64 b) { return a + b; }
}

namespace void_func {
	i64 global_value = 0;

	void voidTester(i64 a) {
		std::cerr << "called void_tester with a = " << a << "\n";
		global_value = a;
	}

	i64 counter = 0;

	void voidNoArgs() {
		std::cerr << "called void_no_args\n";
		counter++;
	}
}

namespace cpp_vector {
	std::vector<i64> vec;

	std::vector<i64>* vecSpawn() { return &vec; }

	std::vector<i64>* vecPushBack(std::vector<i64>* vec, i64 value) {
		vec->push_back(value);
		return vec;
	}

	u64 vecSize(std::vector<i64>* vec) { return vec->size(); }
}

namespace global_opaque {
	std::vector<i64> vec;

	void vecPushBack(std::vector<i64>* vec, i64 value) { vec->push_back(value); }

	u64 vecSize(std::vector<i64>* vec) { return vec->size(); }
}

namespace hybrid {
	i64 addValue(vm::extc::Value<"i64"> a, i64 b) { return a.value.readBytes<i64>() + b; }

	std::tuple<i64, i64> divMod(i64 a, i64 b) { return { a / b, a % b }; }

	std::tuple<i64, i64> divModValue(vm::extc::Value<"i64"> a, i64 b) {
		return divMod(a.value.readBytes<i64>(), b);
	}

	std::expected<i64, vm::ExtCError> failRaw(i64 /*unused*/) {
		return std::unexpected(vm::ExtCError{ "boom raw" });
	}

	std::expected<i64, vm::ExtCError> failValue(vm::extc::Value<"i64"> /*unused*/) {
		return std::unexpected(vm::ExtCError{ "boom value" });
	}

	i64 throwing(i64 /*unused*/) { throw std::runtime_error("oops"); }

	i64 readUserData(vm::ExtCContext& ctx) { return ctx.userData<i64>(); }

	/// Returns an `i32` value for its `i64` result.
	vm::ExtCReturn wrongResultType(vm::ExtCContext& ctx, vm::ExtCArgs /*args*/) {
		auto value = ctx.createValue(base::StrID("i32"));
		if (!value.has_value()) return std::unexpected(value.error());
		vm::ExtCResults results;
		results.push_back(std::move(*value));
		return results;
	}

	/// A value no VM created.
	class ForeignValue final: public vm::IVMValue {
	public:
		[[nodiscard]] base::CRef<vm::code::valid_type::ValidType> getType() const override {
			throw std::logic_error("unused");
		}

		[[nodiscard]] vm::code::valid_type::ValidTypeID getTypeID() const override {
			throw std::logic_error("unused");
		}

		[[nodiscard]] Bytes getDataSize() const override { return Bytes(sizeof(bytes)); }

		[[nodiscard]] base::Optional<vm::InterpretedDataVariant> readData() const override {
			return {};
		}

		void importDataFrom(const vm::IVMValue& /*source*/) override {}

		[[nodiscard]] vm::PID getPID() const override { return vm::PID::fromU64(0); }

		[[nodiscard]] byte* getBytes() override { return bytes.data(); }

		[[nodiscard]] const byte* getBytes() const override { return bytes.data(); }

		void dprint(std::ostream& /*out*/, const std::string& /*indent*/) const override {}

	private:
		std::array<byte, 8> bytes{};
	};

	vm::ExtCReturn foreignResult(vm::ExtCContext& /*ctx*/, vm::ExtCArgs /*args*/) {
		vm::ExtCResults results;
		results.push_back(Box<vm::IVMValue>(makeBox<ForeignValue>()));
		return results;
	}

	vm::ExtCRawReturn takesPointer(
		vm::ExtCContext& /*ctx*/, vm::ExtCRawArgs /*args*/, vm::ExtCRawResults /*results*/
	) {
		return {};
	}
}

/**
 * @brief What belongs here: calling C++ functions registered in the VM (`VM_EXT_C_FUNC`) from
 * a program, including void functions and opaque pointers to C++ objects.
 *
 * Calling functions from a shared library goes to `runtime/native/ffi/`.
 */
class VmExternCppTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmExternCppTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simple);
		TESTER_ADD_TEST(cppVectorInVm);
		TESTER_ADD_TEST(globalOpaques);
		TESTER_ADD_TEST(voidTest);
		TESTER_ADD_TEST(voidNoArgsTest);
		TESTER_ADD_TEST(conventionSelection);
		TESTER_ADD_TEST(valueConvention);
		TESTER_ADD_TEST(multiResult);
		TESTER_ADD_TEST(errorBecomesPanic);
		TESTER_ADD_TEST(cppExceptionBecomesPanic);
		TESTER_ADD_TEST(wrongResultTypePanics);
		TESTER_ADD_TEST(foreignResultPanics);
		TESTER_ADD_TEST(userData);
		TESTER_ADD_TEST(rawRejectsNonTriviallyCopyable);
	}

private:
	void simple() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			assertSucceeded(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
			          .types                = {},
			          .global_data          = {},
			          .external_c_functions = { VM_EXT_C_FUNC(simple::add) },
			          .ffi_functions        = {},
			          .object_files         = {} }
				),
				"Registering the C functions"
			);
			assertSucceeded(
				vm::api::loadFiles(pid, { fs::File(path("extern_test.dbc")) }),
				"Load of 'extern_test.dbc'"
			);
			return pid;
		};

		runTestOnVm(get_ext_func_program(), { "1 2" }, { "3" });
		runTestOnVm(get_ext_func_program(), { "5 3" }, { "8" });
	}

	void cppVectorInVm() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			assertSucceeded(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
					  .types                = {},
					  .global_data          = {},
					  .external_c_functions = {
						  VM_EXT_C_FUNC(cpp_vector::vecSpawn),
						  VM_EXT_C_FUNC(cpp_vector::vecPushBack),
						  VM_EXT_C_FUNC(cpp_vector::vecSize),
					  },
					  .ffi_functions        = {},
					  .object_files         = {} }
				),
				"Registering the C functions"
			);
			assertSucceeded(
				vm::api::loadFiles(pid, { fs::File(path("cpp_vector_in_vm.dbc")) }),
				"Load of 'cpp_vector_in_vm.dbc'"
			);
			return pid;
		};
		runTestOnVm(get_ext_func_program(), { "123" }, { "1" });
		ASSERT_EQUAL_PRINT(cpp_vector::vec.size(), 1);
		ASSERT_EQUAL(cpp_vector::vec[0], 123);
	}

	void globalOpaques() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			assertSucceeded(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
					  .types                = {},
					  .global_data          = {},
					  .external_c_functions = {
						  VM_EXT_C_FUNC(global_opaque::vecPushBack),
						  VM_EXT_C_FUNC(global_opaque::vecSize),
					  },
					  .ffi_functions        = {},
					  .object_files         = {} }
				),
				"Registering the C functions"
			);
			assertSucceeded(
				vm::api::loadFiles(pid, { fs::File(path("global_opaque.dbc")) }),
				"Load of 'global_opaque.dbc'"
			);
			return pid;
		};

		vm::PID pid = get_ext_func_program();

		// Prepare the initializing argument.
		auto vm_value_response = vm::api::getVMValue(pid, "opaque_ptr");
		assertSucceeded(vm_value_response, "Reading the global 'opaque_ptr'");
		auto  vm_value   = std::move(vm_value_response->vm_value);
		auto* vector_ptr = &global_opaque::vec;
		vm_value->writeBytes(vector_ptr);

		// Initialize the global vector pointer
		assertSucceeded(
			vm::api::runFunction(pid, "initialize_vector", { vm_value.refMut() }),
			"Running initialize_vector"
		);
		assertSucceeded(vm::api::join(pid), "Joining initialize_vector");

		assertSucceeded(vm::api::freeVMValue(pid, vm_value.refMut()), "freeVMValue");

		runTestOnVm(pid, { "1 2 3" }, { "3" });
		ASSERT_EQUAL_PRINT(global_opaque::vec.size(), 3);
		ASSERT_EQUAL(global_opaque::vec[0], 1);
		ASSERT_EQUAL(global_opaque::vec[1], 2);
		ASSERT_EQUAL(global_opaque::vec[2], 3);
	}

	void voidTest() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			assertSucceeded(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
					  .types                = {},
					  .global_data          = {},
					  .external_c_functions = {
						  VM_EXT_C_FUNC_AS("void_tester", void_func::voidTester),
					  },
					  .ffi_functions        = {},
					  .object_files         = {} }
				),
				"Registering the C functions"
			);
			assertSucceeded(
				vm::api::loadFiles(pid, { fs::File(path("void_func_test.dbc")) }),
				"Load of 'void_func_test.dbc'"
			);
			return pid;
		};
		runTestOnVm(get_ext_func_program(), { "456" }, {});
		ASSERT_EQUAL(void_func::global_value, 456);
	}

	void voidNoArgsTest() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			assertSucceeded(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
					  .types                = {},
					  .global_data          = {},
					  .external_c_functions = {
						  VM_EXT_C_FUNC_AS("void_no_args", void_func::voidNoArgs),
					  },
					  .ffi_functions        = {},
					  .object_files         = {} }
				),
				"Registering the C functions"
			);
			assertSucceeded(
				vm::api::loadFiles(pid, { fs::File(path("void_no_args.dbc")) }),
				"Load of 'void_no_args.dbc'"
			);
			return pid;
		};
		runTestOnVm(get_ext_func_program(), {}, {});
		ASSERT_EQUAL(void_func::counter, 3);
	}

	/**
	 * @brief A program calling `native` with the immediate `args` (all `i64`) and printing its
	 * `result_count` results (all `i64`).
	 */
	static fs::File callProgram(std::string_view native, usize result_count, std::vector<i64> args) {
		std::string body = "function main { i64, ptr_argv } -> { i64 } {\n";
		for (usize i = 0; i < result_count; i++)
			body += base::strConcat("    init_pany_type r", i, ", i64;\n");
		for (usize i = 0; i < args.size(); i++) {
			body += base::strConcat("    init_pany_type a", i, ", i64;\n");
			body += base::strConcat("    mov_p64_imm a", i, ", ", args[i], ";\n");
		}
		body += base::strConcat("    call_cfunc ", native, ";\n");
		for (usize i = 0; i < result_count; i++)
			body += base::strConcat("    output_p64 r", i, ";\n");
		for (usize i = 0; i < result_count; i++) body += "    deinit;\n";
		body += "    mov_p64_imm ret0, 0;\n    deinit;\n    deinit;\n    ret;\n}\n";
		return inlineDbc(native, body);
	}

	vm::PID loadWith(std::vector<vm::code::ExternalCFunction> functions, const fs::File& program) {
		auto pid = initProcess();
		assertSucceeded(
			vm::api::loadCode(
				pid,
				{ .functions            = {},
		          .types                = {},
		          .global_data          = {},
		          .external_c_functions = std::move(functions),
		          .ffi_functions        = {},
		          .object_files         = {} }
			),
			"Registering the C functions"
		);
		assertSucceeded(vm::api::loadFiles(pid, { program }), "Loading the program");
		return pid;
	}

	/// Typed functions use the raw convention, unless a parameter is a `Value`.
	void conventionSelection() {
		ASSERT_TRUE(
			std::holds_alternative<vm::ExtCRawFunctionPtr>(VM_EXT_C_FUNC(simple::add).function)
		);
		ASSERT_TRUE(std::holds_alternative<vm::ExtCValueFunctionPtr>(
			VM_EXT_C_FUNC(hybrid::addValue).function
		));
	}

	void valueConvention() {
		runTestOnVm(
			loadWith(
				{ VM_EXT_C_FUNC_AS("add_value", hybrid::addValue) },
				callProgram("add_value", 1, { 40, 2 })
			),
			{},
			{ "42" }
		);
	}

	void multiResult() {
		runTestOnVm(
			loadWith(
				{ VM_EXT_C_FUNC_AS("div_mod", hybrid::divMod) }, callProgram("div_mod", 2, { 17, 5 })
			),
			{},
			{ "32" }
		);
		runTestOnVm(
			loadWith(
				{ VM_EXT_C_FUNC_AS("div_mod_value", hybrid::divModValue) },
				callProgram("div_mod_value", 2, { 17, 5 })
			),
			{},
			{ "32" }
		);
	}

	void errorBecomesPanic() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(loadWith(
				{ VM_EXT_C_FUNC_AS("fail_raw", hybrid::failRaw) }, callProgram("fail_raw", 1, { 1 })
			)),
			"'fail_raw': boom raw"
		);
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(loadWith(
				{ VM_EXT_C_FUNC_AS("fail_value", hybrid::failValue) },
				callProgram("fail_value", 1, { 1 })
			)),
			"'fail_value': boom value"
		);
	}

	void cppExceptionBecomesPanic() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(loadWith(
				{ VM_EXT_C_FUNC_AS("throwing", hybrid::throwing) }, callProgram("throwing", 1, { 1 })
			)),
			"C++ exception: oops"
		);
	}

	vm::code::ExternalCFunction handWritten(
		std::string_view                  name,
		vm::ExtCFunction                  function,
		std::vector<vm::code::Identifier> parameters
	) {
		return { .name      = vm::code::Identifier(base::StrID(std::string(name))),
			     .function  = function,
			     .user_data = nullptr,
			     .signature
			     = { .result_types = { base::StrID("i64") }, .parameters = std::move(parameters) },
			     .parameter_sizes = {},
			     .result_sizes    = {} };
	}

	void wrongResultTypePanics() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(loadWith(
				{ handWritten("wrong_type", &hybrid::wrongResultType, {}) },
				callProgram("wrong_type", 1, {})
			)),
			"result 0 has type 'i32', expected 'i64'"
		);
	}

	/// A result the VM did not create is refused with a panic, not a crash.
	void foreignResultPanics() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult(loadWith(
				{ handWritten("foreign", &hybrid::foreignResult, {}) }, callProgram("foreign", 1, {})
			)),
			"result 0 does not belong to this process"
		);
	}

	void userData() {
		i64 value = 77;
		runTestOnVm(
			loadWith(
				{ vm::extc::function<&hybrid::readUserData>(base::StrID("read_user_data"), &value) },
				callProgram("read_user_data", 1, {})
			),
			{},
			{ "77" }
		);
	}

	/// The raw convention copies bytes, so it cannot take a VM pointer.
	void rawRejectsNonTriviallyCopyable() {
		auto pid    = initProcess();
		auto loaded = vm::api::loadCode(
			pid,
			{ .functions   = {},
		      .types       = {},
		      .global_data = {},
		      .external_c_functions
		      = { handWritten("takes_pointer", &hybrid::takesPointer, { base::StrID("ptr_i64") }) },
		      .ffi_functions = {},
		      .object_files  = {} }
		);
		ASSERT_NO_VALUE(loaded);
		ASSERT_TRUE(vm::api::errorToString(loaded.error()).contains("not trivially copyable"));
		assertSucceeded(vm::api::kill(pid), "Killing the process");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/native/extern_c/");

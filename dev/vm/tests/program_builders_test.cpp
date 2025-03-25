/**
 * @file dvm_backend_unit_tests.cpp
 */
#include <tester/tester.hpp>
#include <vm/code/code.hpp>
#include <vm/code/utils.hpp>
#include <base/string_id.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/code/instructions.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/code/builders/builders.hpp>
#include <vm/code/serializer/serializer.hpp>
#include <sstream>

using namespace vm::code::builders;
using namespace vm::code::instructions;
using vm::code::Instruction;

class DVMBackendUnitTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DVMBackendUnitTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testInstructionBuilder);
		TESTER_ADD_TEST(testFunctionBuilder);
		TESTER_ADD_TEST(testFileBuilder);
	}


private:
	template<class T>
	void assertInstrEq(Instruction instruction1, T instruction2) {
		ASSERT_TRUE(std::holds_alternative<T>(instruction1));
		ASSERT_TRUE(vm::code::utils::areInstrEqual(std::get<T>(instruction1), instruction2));
	}

	void testInstructionBuilder() {
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built instruction without set kind"
		);

		InstructionBuilder instr_builder;

		// Test `mov_l32_l32`
		instr_builder.setKind(OpKind::mov);
		const auto arg0 = vm::opargs::StackLocalI32{ 0 };
		const auto arg1 = vm::opargs::StackLocalI32{ 4 };
		const auto arg2 = vm::opargs::Immediate{ 10 };
		instr_builder.pushArgs(arg0, arg1);
		std::vector<Instruction> instr = instr_builder.build();
		ASSERT_TRUE(instr.size() == 1);
		assertInstrEq(instr[0], Op_mov_l32_l32{ arg0, arg1 });

		// Test `ret_l32_l32` does not exist
		instr_builder.setKind(vm::code::builders::OpKind::ret);
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built an invalid instruction"
		);

		// Test a = b + c
		// "add a b c" is supported by the builder
		// Should output:
		// mov a b  # a = b
		// add a c  # a += c
		instr_builder.setKind(vm::code::builders::OpKind::add);
		instr_builder.pushArg(arg2);
		std::vector<Instruction> instr1 = instr_builder.build();
		ASSERT_TRUE(instr1.size() == 2);
		assertInstrEq(instr1[0], Op_mov_l32_l32{ arg0, arg1 });
		assertInstrEq(instr1[1], Op_add_l32_imm{ arg0, arg2 });

		// Test whether `a = a + b` resolves to `add a b`.
		InstructionBuilder instr_builder1(OpKind::add);
		instr_builder1.pushArgs(arg0, arg0, arg2);
		auto instr2 = instr_builder1.build();
		ASSERT_EQUAL(1, instr2.size());
		assertInstrEq(instr2[0], Op_add_l32_imm{ arg0, arg2 });
	}

	void testFunctionBuilder() {
		base::HashMap<base::StrID, vm::code::TypeOfData> available_types;
		auto                                             int32 = base::StrID("int32");
		auto                                             int64 = base::StrID("int64");
		available_types.put(int32, vm::code::PrimitiveType{ int32, 4 });
		available_types.put(int64, vm::code::PrimitiveType{ int64, 8 });
		FunctionBuilder func_builder(base::StrID("test"), available_types);

		auto a = func_builder.initType(int32);
		ASSERT_EQUAL(0, a);
		auto b = func_builder.initType(int64);
		auto c = func_builder.initType(int64);
		ASSERT_EQUAL(4, b);
		ASSERT_EQUAL(12, c);

		InstructionBuilder instr(OpKind::add);
		const auto         arg0 = vm::opargs::StackLocalI32{ i64(a) };
		const auto         arg1 = vm::opargs::Immediate{ 7 };
		instr.pushArgs(arg0, arg1);

		func_builder.addInstruction(instr);

		func_builder.deinitType();
		func_builder.deinitType();
		auto d = func_builder.initType(int32);
		ASSERT_EQUAL(4, d);
		func_builder.addInstruction(InstructionBuilder(OpKind::ret));
		vm::code::Function func = func_builder.build();
		ASSERT_EQUAL(base::StrID("test"), func.name);
		ASSERT_EQUAL(20, func.local_stack_size);
		ASSERT_EQUAL(0, func.ret_size);
		ASSERT_EQUAL(0, func.arg_size);
		ASSERT_EQUAL(0, func.next_arg_size);

		assertInstrEq(func.body[0], Op_init_type{ int32 });
		assertInstrEq(func.body[1], Op_init_type{ int64 });
		assertInstrEq(func.body[2], Op_init_type{ int64 });
		assertInstrEq(func.body[3], Op_add_l32_imm{ arg0, arg1 });
		assertInstrEq(func.body[4], Op_deinit{});
		assertInstrEq(func.body.back(), Op_ret{});
	}

	void testFileBuilder() {
		CodeFileBuilder file_builder;

		auto int32 = base::StrID("int32");
		auto int64 = base::StrID("int64");
		file_builder.addType(vm::code::PrimitiveType(int32, 4));
		file_builder.addType(vm::code::PrimitiveType(int64, 8));

		FunctionBuilder func_builder(base::StrID("main"), file_builder.getAvailableTypes());
		auto            a = func_builder.initType(int64);
		func_builder.initType(int32);

		InstructionBuilder instr_mov(OpKind::mov);
		instr_mov.pushArgs(vm::opargs::StackLocalI64(i64(a)), vm::opargs::Immediate( 1'337 ));
		func_builder.addInstruction(instr_mov);
		InstructionBuilder instr_output(OpKind::output);
		instr_output.pushArgs(vm::opargs::StackLocalI64(i64(a)));
		func_builder.addInstruction(instr_output);
		func_builder.deinitType();
		func_builder.deinitType();
		assertThrows<base::Panic>(
			[&] { func_builder.deinitType(); }, "Cannot pop from empty variable stack"
		);

		func_builder.addInstruction(InstructionBuilder(OpKind::ret));

		file_builder.addFunction(func_builder);
		vm::code::CodeFile code = file_builder.build();

		std::stringstream ss;
		vm::code::serialize(code, ss);
		std::string serialized = ss.str();
		std::cerr << serialized << '\n';

		// This is deterministic, so we can check for this.
		ASSERT_TRUE(serialized.starts_with("type primitive: int32 4"));

		// @TODO: Test that built file parses well.
	}
};

TESTER_COMMON_MAIN("/compiler/backends/dvm/tests")

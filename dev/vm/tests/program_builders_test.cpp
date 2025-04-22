/**
 * @file dvm_backend_unit_tests.cpp
 */
#include <tester/tester.hpp>

#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>

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
	void assertInstructionsEqual(Instruction instruction1, T instruction2) {
		ASSERT_EQUAL(instruction1, Instruction{ instruction2 });
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
		instr_builder.pushArgs(arg0, arg1);
		std::vector<Instruction> instr = instr_builder.build();
		ASSERT_TRUE(instr.size() == 1);
		assertInstructionsEqual(instr[0], Op_mov_l32_l32{ arg0, arg1 });

		// Test `ret_l32_l32` does not exist
		instr_builder.setKind(vm::code::builders::OpKind::ret);
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built an invalid instruction"
		);
	}

	void testFunctionBuilder() {
		TypeContextBuilder type_context_builder;
		auto               void_t = base::StrID("void");
		auto               int32  = base::StrID("int32");
		auto               int64  = base::StrID("int64");
		auto               test   = base::StrID("test");
		type_context_builder.addType(vm::code::PrimitiveType{ void_t, 0 });
		type_context_builder.addType(vm::code::PrimitiveType{ int32, 4 });
		type_context_builder.addType(vm::code::PrimitiveType{ int64, 8 });
		type_context_builder.addType(vm::code::FunctionType{ test, {}, { void_t } });

		TypeContext     available_types = type_context_builder.build();
		FunctionBuilder func_builder(test, available_types);

		auto a = func_builder.initType(Op_init_type{ int32 });
		ASSERT_EQUAL(0, a);
		auto b = func_builder.initType(Op_init_type{ int64 });
		auto c = func_builder.initType(Op_init_type{ int64 });
		ASSERT_EQUAL(4, b);
		ASSERT_EQUAL(12, c);

		InstructionBuilder instr(OpKind::add);
		const auto         arg0 = vm::opargs::StackLocalI32{ i64(a) };
		const auto         arg1 = vm::opargs::Immediate{ 7 };
		instr.pushArgs(arg0, arg1);

		func_builder.addInstruction(instr);

		func_builder.addInstruction(Op_deinit());
		func_builder.addInstruction(Op_deinit());
		auto d = func_builder.initType(Op_init_type{ int32 });
		ASSERT_EQUAL(4, d);
		func_builder.addInstruction(InstructionBuilder(OpKind::ret));
		vm::code::Function func = func_builder.build();
		ASSERT_EQUAL(base::StrID("test"), func.name);
		ASSERT_EQUAL(20, func.local_stack_size);

		assertInstructionsEqual(func.body[0], Op_init_type{ int32 });
		assertInstructionsEqual(func.body[1], Op_init_type{ int64 });
		assertInstructionsEqual(func.body[2], Op_init_type{ int64 });
		assertInstructionsEqual(func.body[3], Op_add_l32_imm{ arg0, arg1 });
		assertInstructionsEqual(func.body[4], Op_deinit{});
		assertInstructionsEqual(func.body.back(), Op_ret{});
	}

	void testFileBuilder() {
		TypeContextBuilder type_context_builder;

		auto main  = base::StrID("main");
		auto int32 = base::StrID("int32");
		auto int64 = base::StrID("int64");
		type_context_builder.addType(vm::code::PrimitiveType(int32, 4));
		type_context_builder.addType(vm::code::PrimitiveType(int64, 8));
		type_context_builder.addType(vm::code::FunctionType(main, {}, int64));

		TypeContext finalized = type_context_builder.build();

		FunctionBuilder func_builder(main, finalized);
		auto            a = func_builder.initType(Op_init_type{ int64 });  // a
		func_builder.initType(Op_init_type{ int32 });                      // b

		InstructionBuilder instr_mov(OpKind::mov);
		instr_mov.pushArgs(vm::opargs::StackLocalI64(i64(a)), vm::opargs::Immediate(1'337));
		func_builder.addInstruction(instr_mov);
		InstructionBuilder instr_output(OpKind::output);
		instr_output.pushArgs(vm::opargs::StackLocalI64(i64(a)));
		func_builder.addInstruction(instr_output);
		func_builder.addInstruction(Op_deinit());  // a
		func_builder.addInstruction(Op_deinit());  // b
		func_builder.addInstruction(Op_deinit());  // ret val (int64)
		assertThrows<EmptyStackDeinitError>(
			[&] { func_builder.addInstruction(Op_deinit()); }, "Cannot pop from empty variable stack"
		);

		func_builder.initType(Op_init_type{ int64 });  // reinit ret val (int64)
		func_builder.addInstruction(InstructionBuilder(OpKind::ret));

		std::stringstream ss;
		for (const auto& type: finalized.getTypes()) vm::code::serialize(type, ss);
		auto func = func_builder.build();
		vm::code::serialize(func, ss);
		std::string serialized = ss.str();
		std::cerr << serialized << '\n';

		// This is deterministic, so we can check for this.
		ASSERT_TRUE(serialized.starts_with("type primitive: int32 4"));

		// @TODO: Test that built file parses well.
	}
};

TESTER_COMMON_MAIN("/compiler/backends/dvm/tests")

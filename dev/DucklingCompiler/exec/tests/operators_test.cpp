#include <exec/exec.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>
#include <exec/operators/builtinoperators.hpp>
#include <exec/ctv.hpp>
#include <operations/operation.hpp>
using namespace exec;

class SimpleExecTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleExecTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Exec Test") {
		exec::init();

		TESTER_ADD_TEST(initialization);
		TESTER_ADD_TEST(simple_int_test);
	}

private:
	void initialization() { assert(getBuiltInOps().notEmpty(), "Built in operator map is empty."); }

	void simple_int_test() {
		auto           int_type = query::entryPoint<ts::QueryIntegralType>({ 8 });
		ts::TypeDesc<> int_desc{ int_type };

		auto eq_int = [](std::vector<exec::CTV> ctvs) {
			int8_t a   = ctvs[0].getData<int8_t>().front();
			int8_t b   = ctvs[1].getData<int8_t>().front();
			bool   res = (a == b);
			return res;
		};

		exec::CTV int_ctv_a                   = exec::alloc_new(int_desc, int_type.getSize());
		exec::CTV int_ctv_b                   = exec::alloc_new(int_desc, int_type.getSize());
		exec::CTV int_ctv_exp                 = exec::alloc_new(int_desc, int_type.getSize());
		int_ctv_a.getData<int8_t>().front()   = 9;
		int_ctv_b.getData<int8_t>().front()   = 4;
		int_ctv_exp.getData<int8_t>().front() = 13;
		BuiltInOp add_op                      = { Operator::Plus, { int_desc, int_desc } };
		assert(getBuiltInOps().contains(add_op), "Int8 addition not found in built in operations.");
		auto add_id = getBuiltInOps()[add_op];
		assert(operation::existsOperation(add_id), "Int8 addition not found in operations.");
		auto      add_typed_op = operation::getOperation(add_id);
		exec::CTV int_ctv_r    = add_typed_op({ int_ctv_a, int_ctv_b });
		assert(eq_int({ int_ctv_r, int_ctv_exp }), "Wrong result of addition. Expected 9+4=13");

		int_ctv_exp.getData<int8_t>().front() = 5;
		BuiltInOp sub_op                      = { Operator::Minus, { int_desc, int_desc } };
		assert(
			getBuiltInOps().contains(sub_op), "Int8 subtraction not found in built in operations."
		);
		auto sub_id = getBuiltInOps()[sub_op];
		assert(operation::existsOperation(sub_id), "Int8 subtraction not found in operations.");
		auto sub_typed_op = operation::getOperation(sub_id);
		int_ctv_r         = sub_typed_op({ int_ctv_a, int_ctv_b });
		assert(eq_int({ int_ctv_r, int_ctv_exp }), "Wrong result of subtraction. Expected 9-4=5");

		int_ctv_exp.getData<int8_t>().front() = 36;
		BuiltInOp mul_op                      = { Operator::Asterisk, { int_desc, int_desc } };
		assert(
			getBuiltInOps().contains(mul_op),
			"Int8 multiplication not found in built in operations."
		);
		auto mul_id = getBuiltInOps()[mul_op];
		assert(operation::existsOperation(mul_id), "Int8 multiplication not found in operations.");
		auto mul_typed_op = operation::getOperation(mul_id);
		int_ctv_r         = mul_typed_op({ int_ctv_a, int_ctv_b });
		assert(
			eq_int({ int_ctv_r, int_ctv_exp }), "Wrong result of multiplication. Expected 9*4=36"
		);

		int_ctv_exp.getData<int8_t>().front() = 2;
		BuiltInOp div_op                      = { Operator::Slash, { int_desc, int_desc } };
		assert(getBuiltInOps().contains(div_op), "Int8 division not found in built in operations.");
		auto div_id = getBuiltInOps()[div_op];
		assert(operation::existsOperation(div_id), "Int8 division not found in operations.");
		auto div_typed_op = operation::getOperation(div_id);
		int_ctv_r         = div_typed_op({ int_ctv_a, int_ctv_b });
		assert(eq_int({ int_ctv_r, int_ctv_exp }), "Wrong result of division. Expected 9/4=2");
	}

public:
	~SimpleExecTest() override = default;
};

TESTER_COMMON_MAIN("/DucklingCompiler/src/exec/tests/")

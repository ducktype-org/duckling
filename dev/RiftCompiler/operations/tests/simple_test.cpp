#include <exec/ctv.hpp>
#include <operations/operation.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>

class OperationsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OperationsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Operation Test") {
		TESTER_ADD_TEST(simple_operation);
		TESTER_ADD_TEST(simple_default);
	}

private:
	void simple_operation() {
		ts::TypeDesc<> td(query::entryPoint<ts::QueryIntegralType>({ 8 }));
		exec::CTV      ctv = exec::alloc_new(td, 8);

		auto int_16 = query::entryPoint<ts::QueryIntegralType>({ 16 });

		auto fun_sig = query::entryPoint<ts::QueryFunctionType>({ { int_16 }, int_16 });

		operation::Operation      fun = [](std::vector<exec::CTV> a) { return a[0]; };
		operation::TypedOperation op  = { fun, fun_sig };


		auto id     = operation::addOperation(op);
		auto get_op = operation::getOperation(id);


		assert(get_op.signature == fun_sig, "Wrong signature.");

		auto res = get_op.function(std::vector<exec::CTV>{ ctv });

		assert(res.data == ctv.data, "Wrong CTV result,");
		assert(res.size == ctv.size, "Wrong CTV size.");
	}

	void simple_default() {
		ts::TypeDesc<> td(query::entryPoint<ts::QueryIntegralType>({ 8 }));
		exec::CTV      ctv = exec::alloc_new(td, 8);

		auto int_16 = query::entryPoint<ts::QueryIntegralType>({ 16 });

		auto fun_sig = query::entryPoint<ts::QueryFunctionType>({ { int_16 }, int_16 });

		operation::Operation      fun = [](std::vector<exec::CTV> a) { return a[0]; };
		operation::TypedOperation op  = { fun, fun_sig };

		auto id = operation::addDefault(operation::Defaultable::Compare, int_16, op);

		auto def_id = operation::getIDDefault(operation::Defaultable::Compare, int_16);


		assert(id == def_id, "IDs of the same operation do not match.");

		auto get_op = operation::getOperation(id);


		assert(get_op.signature == fun_sig, "Wrong signature.");

		auto res = get_op.function(std::vector<exec::CTV>{ ctv });

		assert(res.data == ctv.data, "Wrong CTV result,");
		assert(res.size == ctv.size, "Wrong CTV size.");

		const operation::TypedOperation& get_op2
			= operation::getDefault(operation::Defaultable::Compare, int_16);

		auto res2 = get_op2.function(std::vector<exec::CTV>{ ctv });

		assert(get_op2.signature == fun_sig, "Wrong signature.");
		assert(res2.data == ctv.data, "Wrong CTV result,");
		assert(res2.size == ctv.size, "Wrong CTV size.");
	}

public:
	~OperationsTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/operations/tests/")

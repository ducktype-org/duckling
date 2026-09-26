#include <c_import/c_decls.hpp>
#include <c_import/type_mapper.hpp>

#include <tester/tester.hpp>

#include <string>

using namespace c_import;

class CImportTypesTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportTypesTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(scalarsTest);
		TESTER_ADD_TEST(pointersTest);
		TESTER_ADD_TEST(arraysTest);
		TESTER_ADD_TEST(unsupportedTest);
	}

private:
	void scalarsTest() {
		ASSERT_EQUAL(std::string("i8"), renderType(makeScalar(Scalar::Int8)));
		ASSERT_EQUAL(std::string("i32"), renderType(makeScalar(Scalar::Int32)));
		ASSERT_EQUAL(std::string("i64"), renderType(makeScalar(Scalar::Int64)));
		ASSERT_EQUAL(std::string("u8"), renderType(makeScalar(Scalar::UInt8)));
		ASSERT_EQUAL(std::string("u64"), renderType(makeScalar(Scalar::UInt64)));
		ASSERT_EQUAL(std::string("f32"), renderType(makeScalar(Scalar::Float32)));
		ASSERT_EQUAL(std::string("f64"), renderType(makeScalar(Scalar::Float64)));
		ASSERT_EQUAL(std::string("bool"), renderType(makeScalar(Scalar::Bool)));
		ASSERT_EQUAL(std::string("char"), renderType(makeScalar(Scalar::Char)));
	}

	void pointersTest() {
		// C `void*` and pointers to anything skipped both degrade to `cptr u8`.
		ASSERT_EQUAL(std::string("cptr u8"), renderType(makeOpaquePointer()));
		ASSERT_EQUAL(std::string("cptr u8"), renderType(makePointer(makeUnsupported("union"))));

		ASSERT_EQUAL(std::string("cptr char"), renderType(makePointer(makeScalar(Scalar::Char))));
		ASSERT_EQUAL(std::string("cptr i32"), renderType(makePointer(makeScalar(Scalar::Int32))));
		ASSERT_EQUAL(
			std::string("cptr struct_Node"), renderType(makePointer(makeRecord("struct_Node")))
		);

		// A pointer stays usable even when its pointee is not, so it is never itself skipped.
		ASSERT_TRUE(isSupported(makePointer(makeUnsupported("union"))));
	}

	void arraysTest() {
		ASSERT_EQUAL(std::string("i32[4]"), renderType(makeArray(makeScalar(Scalar::Int32), 4)));

		// A zero-length array has no admissible spelling, so its record is skipped with it.
		ASSERT_EQUAL(false, isSupported(makeArray(makeScalar(Scalar::Int32), 0)));
		ASSERT_EQUAL(false, unsupportedReason(makeArray(makeScalar(Scalar::Int32), 0)).empty());

		// Contamination travels through the element type.
		ASSERT_EQUAL(false, isSupported(makeArray(makeUnsupported("bitfield"), 2)));
	}

	void unsupportedTest() {
		ASSERT_EQUAL(false, isSupported(makeUnsupported("union")));
		ASSERT_EQUAL(std::string("union"), unsupportedReason(makeUnsupported("union")));

		ASSERT_TRUE(isSupported(makeScalar(Scalar::Int32)));
		ASSERT_TRUE(unsupportedReason(makeScalar(Scalar::Int32)).empty());
		ASSERT_TRUE(isSupported(makeRecord("struct_Point")));
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/c_import_types_test.cpp")

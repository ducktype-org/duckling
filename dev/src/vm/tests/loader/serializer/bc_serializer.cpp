// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/types/floats.hpp>

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/loader/loader.hpp>

#include <cstring>
#include <sstream>

using namespace vm::code;
using namespace vm::loader;
using namespace vm;

class SerializeParseRoundtripTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SerializeParseRoundtripTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(roundtripMinimalProgram);
		TESTER_ADD_TEST(roundtripFromString);
	}

private:
	/**
	 * @brief Creates a minimal CodeCollection with 2 types, 1 global with initial value, 1 function.
	 */
	CodeCollection makeMinimalProgram() {
		CodeCollection code;

		// Type 1: primitive i64 (8 bytes)
		code.types.emplace_back(PrimitiveType(base::StrID("i32"), Bytes{ 4 }));
		// Type 2: data Point { x: i64, y: i64 }
		auto point = DataType(
			base::StrID("Point"),
			{ Field(base::StrID("x"), base::StrID("i32")),
		      Field(base::StrID("y"), base::StrID("i32")) }
		);
		point.packed      = true;
		point.assert_size = 8;
		code.types.emplace_back(std::move(point));
		code.types.emplace_back(FixedSizeTableType(base::StrID("points"), base::StrID("Point"), 3));
		// C pointers: one typed, one with an unknown pointee (no inner).
		code.types.emplace_back(CPointerType(base::StrID("point_ptr"), base::StrID("Point")));
		code.types.emplace_back(CPointerType(base::StrID("raw_ptr"), {}));

		// Global data: constant answer i64 with initial_value 42
		GlobalData global;
		global.name        = Identifier(base::StrID("answers"));
		global.type        = Identifier(base::StrID("points"));
		global.is_constant = true;

		auto constant_immediate     = makeBox<ConstantImmediate>();
		constant_immediate->content = ConstantImmediate::fromValue(1).content;
		constant_immediate->size    = ConstantImmediate::fromValue(1).size;
		auto constant_class         = makeBox<ConstantClass>();
		constant_class->fields.emplace_back(base::StrID("x"), constant_immediate->clone());
		constant_class->fields.emplace_back(base::StrID("y"), constant_immediate->clone());
		auto constant_array = makeBox<ConstantFixedSizeTable>();
		constant_array->elements.emplace_back(constant_class->clone());
		constant_array->elements.emplace_back(constant_class->clone());
		constant_array->elements.emplace_back(constant_class->clone());

		global.initial_value = ConstantValue::fromData(std::move(constant_array));
		code.global_data.push_back(std::move(global));

		// FFI: a bare library name is handed to `dlopen` untouched, so it survives the round-trip
		// verbatim (a relative path would be resolved against the source file).
		code.object_files.emplace_back("libm.so.6");

		FFIFunction ffi_func;
		ffi_func.name = Identifier(base::StrID("sqrt"));
		ffi_func.signature.parameters.emplace_back(base::StrID("f64"));
		ffi_func.signature.result_types.emplace_back(base::StrID("f64"));
		code.ffi_functions.push_back(std::move(ffi_func));

		FFIFunction ffi_void_func;
		ffi_void_func.name = Identifier(base::StrID("abort"));
		code.ffi_functions.push_back(std::move(ffi_void_func));

		Function func;
		func.name = Identifier(base::StrID("main"));
		func.signature.parameters.emplace_back(base::StrID("i64"));
		func.signature.parameters.emplace_back(base::StrID("ptr_argv"));
		func.signature.result_types.emplace_back(base::StrID("i64"));
		func.body.emplace_back(
			instructions::Op_mov_p64_imm(opargs::Place64(base::StrID("ret0")), opargs::Immediate(0))
		);
		func.body.emplace_back(instructions::Op_ret());
		code.functions.push_back(std::move(func));

		return code;
	}

	/**
	 * @brief Serializes and parses back a minimal program, verifying key fields survive round-trip.
	 */
	void roundtripMinimalProgram() {
		CodeCollection original = makeMinimalProgram();

		// Serialize
		std::ostringstream oss;
		serializeCode(original, oss);
		std::string serialized = oss.str();

		// Write to virtual file and parse back
		fs::File vfile  = fs::FileManager::createRandomVirtualFile(serialized, ".dbc");
		auto     result = Loader::parseCodeCollectionFromFiles({ vfile });

		ASSERT_HAS_VALUE(result, "Parsing round-trip should succeed");
		const CodeCollection& parsed = result.value();

		// Check types
		assertEqual(parsed.types.size(), original.types.size(), "Type count should match");
		ASSERT_MATCHES(parsed.types[0], PrimitiveType);
		ASSERT_MATCHES(parsed.types[1], DataType);

		const auto& prim = std::get<PrimitiveType>(parsed.types[0]);
		assertEqual(prim.name, base::StrID("i32"), "Primitive type name should be i32");
		assertEqual(prim.size, Bytes{ 4 }, "Primitive type size should be 8");

		const auto& data = std::get<DataType>(parsed.types[1]);
		assertEqual(data.name, base::StrID("Point"), "Data type name should be Point");
		assertEqual(data.fields.size(), static_cast<usize>(2), "Data type should have 2 fields");
		assertTrue(data.packed, "Data type should stay packed after round-trip");
		assertTrue(
			data.assert_size == base::Optional<usize>(8),
			"Data type should keep assert_size after round-trip"
		);

		const auto& typed_cptr = std::get<CPointerType>(parsed.types[3]);
		assertEqual(typed_cptr.name, base::StrID("point_ptr"), "C pointer name should survive");
		assertTrue(
			typed_cptr.inner == base::Optional<base::StrID>(base::StrID("Point")),
			"C pointer inner should survive round-trip"
		);
		const auto& void_cptr = std::get<CPointerType>(parsed.types[4]);
		assertEqual(void_cptr.name, base::StrID("raw_ptr"), "C pointer name should survive");
		ASSERT_NO_VALUE(void_cptr.inner, "An absent C pointer inner should survive round-trip");

		// Check global data
		assertEqual(
			parsed.global_data.size(), original.global_data.size(), "Global count should match"
		);
		const GlobalData& g = parsed.global_data[0];
		assertEqual(g.name.str, base::StrID("answers"), "Global name should be answer");
		assertEqual(g.type.str, base::StrID("points"), "Global type should be i64");
		assertTrue(g.is_constant, "Global should be constant");
		ASSERT_HAS_VALUE(g.initial_value, "Global should have initial_value");
		auto initial_value = *g.initial_value;
		auto cfst = dynamic_cast<vm::code::ConstantFixedSizeTable*>(initial_value.data.get());
		assertTrue(cfst, "Pointer has null value");
		auto cc = dynamic_cast<vm::code::ConstantClass*>(cfst->elements.at(0).get());
		assertTrue(cc, "Pointer has null value");
		auto cimm = dynamic_cast<vm::code::ConstantImmediate*>(cc->fields.at(0).second.get());
		assertTrue(cimm, "Pointer has null value");
		ASSERT_EQUAL_PRINT(cimm->size.asInt(), 4);
		int decoded_value = 0;
		std::memcpy(&decoded_value, cimm->content.data(), 4);
		ASSERT_EQUAL_PRINT(decoded_value, 1);

		// Check FFI declarations
		assertEqual(
			parsed.object_files.size(),
			original.object_files.size(),
			"Object file count should match"
		);
		assertEqual(
			parsed.object_files[0], std::string("libm.so.6"), "Object file path should match"
		);

		assertEqual(
			parsed.ffi_functions.size(), original.ffi_functions.size(), "FFI count should match"
		);
		const FFIFunction& ffi = parsed.ffi_functions[0];
		assertEqual(ffi.name.str, base::StrID("sqrt"), "FFI function name should be sqrt");
		assertTrue(
			ffi.signature == original.ffi_functions[0].signature,
			"FFI function signature should survive round-trip"
		);
		const FFIFunction& void_ffi = parsed.ffi_functions[1];
		assertEqual(void_ffi.name.str, base::StrID("abort"), "FFI function name should be abort");
		assertTrue(
			void_ffi.signature.parameters.empty() && void_ffi.signature.result_types.empty(),
			"An empty FFI signature should survive round-trip"
		);

		// Check function
		assertEqual(
			parsed.functions.size(), original.functions.size(), "Function count should match"
		);
		const Function& f = parsed.functions[0];
		assertEqual(f.name.str, base::StrID("main"), "Function name should be main");
		assertEqual(f.body.size(), static_cast<usize>(2), "Function should have 2 instructions");
	}

	void roundtripFromString() {
		std::string content = R"(type primitive: i8 1
type primitive: i32 4
type data: Point {
    x: i32,
    y: i32,
    is_ok: i8,
}

type data: PackedPoint {
    x: i32,
    y: i32,
} packed assert_size 8

type fixed_size_table: point_arr Point 2
type cpointer: point_ptr Point
type cpointer: raw_ptr

global_data answers point_arr {
    is_constant: true,
    initial_value: fixed_size_table [ class { x: 0x12345678, y: 0x87654321, is_ok: 0xFF }, class { x: 0x00000001, y: 0xFFFFFFFF, is_ok: 0x0A } ]
}

ffi object "libm.so.6";
ffi function sqrt { i64 } -> { i64 };

function main { i64, ptr_argv } -> { i64 } {
    mov_p64_imm                ret0,        0;
    ret                   ;
}


)";

		fs::File vfile  = fs::FileManager::createRandomVirtualFile(content, ".dbc");
		auto     result = Loader::parseCodeCollectionFromFiles({ vfile });

		ASSERT_HAS_VALUE(result, "Parsing round-trip should succeed");
		const CodeCollection& parsed = result.value();

		std::ostringstream oss;
		serializeCode(parsed, oss);
		std::string serialized = oss.str();
		ASSERT_EQUAL_PRINT(content, serialized);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/roundtrip/");

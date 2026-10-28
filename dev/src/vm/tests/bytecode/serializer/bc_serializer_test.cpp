// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/optional.hpp>
#include <base/types/floats.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <vm/bytecode/builtin_types.hpp>
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

/**
 * @brief What belongs here: the bytecode serializer. A program is serialized back to text and
 * parsed again, and the result must be the same program.
 */
class SerializeParseRoundtripTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SerializeParseRoundtripTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(roundtripMinimalProgram);
		TESTER_ADD_TEST(roundtripFromString);
		TESTER_ADD_TEST(aliasesAreExpanded);
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
			{ Field(base::StrID("x"), TypeExpr(base::StrID("i32"))),
		      Field(base::StrID("y"), TypeExpr(base::StrID("i32"))) }
		);
		point.packed      = true;
		point.assert_size = 8;
		code.types.emplace_back(std::move(point));
		// Pointers are written rather than declared, so they appear in a type that refers to
		// them. `cptr{}` stands for an unknown pointee.
		code.types.emplace_back(DataType(
			base::StrID("Handles"),
			{ Field(base::StrID("managed"), TypeExpr::pointer(TypeExpr{ base::StrID("Point") })),
		      Field(base::StrID("raw"), TypeExpr::cpointer(TypeExpr{ base::StrID("Point") })),
		      Field(base::StrID("unknown"), TypeExpr::cpointer()) }
		));

		// Global data: constant answer i64 with initial_value 42
		GlobalData global;
		global.name        = Identifier(base::StrID("answers"));
		global.type        = TypeExpr::fixtable(TypeExpr{ base::StrID("Point") }, 3);
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
		ffi_func.signature.parameters.emplace_back(TypeExpr(base::StrID("f64")));
		ffi_func.signature.result_types.emplace_back(TypeExpr(base::StrID("f64")));
		code.ffi_functions.push_back(std::move(ffi_func));

		FFIFunction ffi_void_func;
		ffi_void_func.name = Identifier(base::StrID("abort"));
		code.ffi_functions.push_back(std::move(ffi_void_func));

		Function func;
		func.name = Identifier(base::StrID("main"));
		func.signature.parameters.emplace_back(TypeExpr(base::StrID("i64")));
		func.signature.parameters.emplace_back(TypeExpr::pointer(TypeExpr::type(base::StrID("argv"))
		));
		func.signature.result_types.emplace_back(TypeExpr(base::StrID("i64")));
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

		assertTrue(
			parsed.global_data.at(0).type.expr
				== TypeExpr::fixtable(TypeExpr{ base::StrID("Point") }, 3),
			"`fixtable{Point, 3}` should survive the round-trip"
		);

		// Pointer expressions must come back as the very same interned expressions.
		const auto& handles = std::get<DataType>(parsed.types[2]);
		assertEqual(handles.name, base::StrID("Handles"), "Data type name should be Handles");
		assertTrue(
			handles.fields.at(0).type == TypeExpr::pointer(TypeExpr{ base::StrID("Point") }),
			"`ptr{Point}` should survive the round-trip"
		);
		assertTrue(
			handles.fields.at(1).type == TypeExpr::cpointer(TypeExpr{ base::StrID("Point") }),
			"`cptr{Point}` should survive the round-trip"
		);
		assertTrue(
			handles.fields.at(2).type == TypeExpr::cpointer(),
			"`cptr{}` should survive the round-trip"
		);

		// Check global data
		assertEqual(
			parsed.global_data.size(), original.global_data.size(), "Global count should match"
		);
		const GlobalData& g = parsed.global_data[0];
		assertEqual(g.name.str, base::StrID("answers"), "Global name should be answer");
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

type data: Handles {
    managed: ptr{Point},
    raw: cptr{Point},
    unknown: cptr{},
    many: ptr{dyntable{Point}},
    either: variant{i32, ptr{Point}},
}


global_data answers fixtable{Point, 2} {
    is_constant: true,
    initial_value: fixtable [ class { x: 0x12345678, y: 0x87654321, is_ok: 0xFF }, class { x: 0x00000001, y: 0xFFFFFFFF, is_ok: 0x0A } ]
}

ffi object "libm.so.6";
ffi function sqrt { i64 } -> { i64 };

function main { i64, ptr{dyntable{ptr{dyntable{byte}}}} } -> { i64 } {
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

	/// `string` and `argv` never name a type of their own, so they are expanded where read.
	void aliasesAreExpanded() {
		const std::string content = R"(function main { i64, ptr{argv} } -> { i64 } {
    init_pany_type s, ptr{string};
    deinit;
    ret;
}
)";

		fs::File vfile  = fs::FileManager::createRandomVirtualFile(content, ".dbc");
		auto     result = Loader::parseCodeCollectionFromFiles({ vfile });
		ASSERT_HAS_VALUE(result, "Parsing should succeed");

		const auto& main = result.value().functions.at(0);
		ASSERT_TRUE(
			main.signature.parameters.at(1).expr
			== vm::code::TypeExpr::pointer(vm::code::argvTypeExpr())
		);
		ASSERT_EQUAL_PRINT(
			std::string("dyntable{ptr{dyntable{byte}}}"),
			vm::code::argvTypeExpr().canonicalName().str()
		);

		const auto& init = main.body.at(0).get<vm::code::instructions::Op_init_pany_type>();
		ASSERT_TRUE(init.type.expr == vm::code::TypeExpr::pointer(vm::code::stringTypeExpr()));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/serializer/");

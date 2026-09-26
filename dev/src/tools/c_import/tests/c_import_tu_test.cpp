#include <c_import/c_decls.hpp>
#include <c_import/dk_emitter.hpp>
#include <c_import/tu_reader.hpp>
#include <c_import/type_mapper.hpp>

#include <tester/tester.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace c_import;

namespace {

	const CFunction* findFunction(const TranslationUnitModel& model, std::string_view name) {
		for (const auto& decl: model.decls) {
			const auto* function = std::get_if<CFunction>(&decl);
			if (function != nullptr && function->name == name) return function;
		}
		return nullptr;
	}

	const CRecord* findRecord(const TranslationUnitModel& model, std::string_view name) {
		for (const auto& decl: model.decls) {
			const auto* record = std::get_if<CRecord>(&decl);
			if (record != nullptr && record->emitted_name == name) return record;
		}
		return nullptr;
	}

	const CConst* findConst(const TranslationUnitModel& model, std::string_view name) {
		for (const auto& decl: model.decls) {
			const auto* value = std::get_if<CConst>(&decl);
			if (value != nullptr && value->name == name) return value;
		}
		return nullptr;
	}

	const CSkipped* findSkipped(const TranslationUnitModel& model, std::string_view name) {
		for (const auto& decl: model.decls) {
			const auto* skipped = std::get_if<CSkipped>(&decl);
			if (skipped != nullptr && skipped->name == name) return skipped;
		}
		return nullptr;
	}

	std::string renderReturn(const CFunction& function) {
		return function.return_type == nullptr ? "void" : renderType(*function.return_type);
	}

	std::string renderParam(const CFunction& function, std::size_t index) {
		return renderType(function.params.at(index).type);
	}

}

class CImportTuTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportTuTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(scalarsTest);
		TESTER_ADD_TEST(recordsTest);
		TESTER_ADD_TEST(enumsAndMacrosTest);
		TESTER_ADD_TEST(hostileTest);
		TESTER_ADD_TEST(determinismTest);
	}

private:
	TranslationUnitModel read(const std::string& header) {
		const auto result = readTranslationUnit(ReadRequest{ .headers    = { path(header) },
		                                                     .clang_args = {},
		                                                     .std_flag   = "-std=c17",
		                                                     .ignore_parse_errors = false });
		assertTrue(result.error.empty(), "reading " + header + " failed: " + result.error);
		return result.model;
	}

	void scalarsTest() {
		const auto model = read("headers/scalars.h");

		ASSERT_EQUAL(std::string("i32"), renderParam(*findFunction(model, "scalars_int"), 0));
		ASSERT_EQUAL(std::string("u32"), renderParam(*findFunction(model, "scalars_uint"), 0));
		ASSERT_EQUAL(std::string("i16"), renderParam(*findFunction(model, "scalars_short"), 0));
		ASSERT_EQUAL(std::string("i64"), renderParam(*findFunction(model, "scalars_longlong"), 0));
		ASSERT_EQUAL(std::string("f32"), renderParam(*findFunction(model, "scalars_float"), 0));
		ASSERT_EQUAL(std::string("f64"), renderParam(*findFunction(model, "scalars_double"), 0));
		ASSERT_EQUAL(std::string("bool"), renderParam(*findFunction(model, "scalars_bool"), 0));

		// C `char` is sign-sensitive, so it maps to i8/u8 rather than Duckling `char`.
		ASSERT_EQUAL(std::string("i8"), renderParam(*findFunction(model, "scalars_schar"), 0));
		ASSERT_EQUAL(std::string("u8"), renderParam(*findFunction(model, "scalars_uchar"), 0));
		// `char *` keeps the ergonomic spelling.
		ASSERT_EQUAL(
			std::string("cptr char"), renderParam(*findFunction(model, "scalars_strlen"), 0)
		);

		ASSERT_EQUAL(std::string("void"), renderReturn(*findFunction(model, "scalars_nothing")));

		// scalars.h includes <stddef.h>, which only resolves when libclang is given its own
		// builtin header directory.
		ASSERT_EQUAL(std::string("u64"), renderParam(*findFunction(model, "scalars_size"), 0));
	}

	void recordsTest() {
		const auto model = read("headers/records.h");

		const auto* point = findRecord(model, "struct_records_point");
		ASSERT_TRUE(point != nullptr);
		ASSERT_EQUAL(std::size_t{ 2 }, point->fields.size());
		ASSERT_EQUAL(std::string("x"), point->fields[0].name);

		// A self-referential record works because the field goes through a pointer.
		const auto* node = findRecord(model, "struct_records_node");
		ASSERT_TRUE(node != nullptr);
		ASSERT_EQUAL(std::string("cptr struct_records_node"), renderType(node->fields[1].type));

		// A record declared only through a typedef borrows the typedef's name, and the tag
		// prefix is kept so it cannot collide with an ordinary identifier.
		ASSERT_TRUE(findRecord(model, "struct_records_anon_t") != nullptr);

		// An opaque record gets no class, so every use of it has to degrade to `cptr u8`
		// rather than naming a type that was never emitted.
		ASSERT_TRUE(findRecord(model, "struct_records_opaque") == nullptr);
		ASSERT_EQUAL(std::string("cptr u8"), renderReturn(*findFunction(model, "records_open")));

		ASSERT_EQUAL(
			std::string("struct_records_point"), renderParam(*findFunction(model, "records_sum"), 0)
		);
		ASSERT_EQUAL(
			std::string("struct_records_point"), renderReturn(*findFunction(model, "records_make"))
		);

		const auto* array = findRecord(model, "struct_records_array");
		ASSERT_TRUE(array != nullptr);
		ASSERT_EQUAL(std::string("i32[4]"), renderType(array->fields[0].type));
	}

	void enumsAndMacrosTest() {
		const auto model = read("headers/enums_macros.h");

		ASSERT_EQUAL(std::string("128i64"), findConst(model, "EM_MAX")->literal);
		ASSERT_EQUAL(std::string("7u64"), findConst(model, "EM_UNSIGNED")->literal);
		ASSERT_EQUAL(std::string("-3i64"), findConst(model, "EM_NEGATIVE")->literal);
		// Duckling has no hexadecimal literals, so the value is re-rendered in decimal.
		ASSERT_EQUAL(std::string("16i64"), findConst(model, "EM_HEX")->literal);
		ASSERT_EQUAL(std::string("3.5f64"), findConst(model, "EM_PI")->literal);

		// A leading zero means octal in C, so this is 448 rather than 700.
		const auto* octal = findConst(model, "EM_OCTAL");
		ASSERT_TRUE(octal != nullptr);
		ASSERT_EQUAL(std::string("448i64"), octal->literal);

		ASSERT_TRUE(findSkipped(model, "EM_NOT_A_NUMBER") != nullptr);
		// A function-like macro is dropped without a comment.
		ASSERT_TRUE(findConst(model, "EM_FUNCTION_LIKE") == nullptr);

		ASSERT_EQUAL(std::string("7u32"), findConst(model, "EM_BLUE")->literal);
		ASSERT_EQUAL(std::string("u32"), findConst(model, "EM_BLUE")->type);
		// A negative enumerator forces a signed underlying type.
		ASSERT_EQUAL(std::string("-1i32"), findConst(model, "EM_NEG")->literal);

		// Clang predefines several hundred macros; none of them belong to this header.
		ASSERT_TRUE(findConst(model, "__STDC__") == nullptr);
		ASSERT_TRUE(findConst(model, "__x86_64__") == nullptr);
		ASSERT_TRUE(findSkipped(model, "__INTMAX_TYPE__") == nullptr);
	}

	void hostileTest() {
		const auto model = read("headers/hostile.h");

		ASSERT_TRUE(findSkipped(model, "union_hostile_union") != nullptr);
		ASSERT_TRUE(findSkipped(model, "struct_hostile_bits") != nullptr);
		ASSERT_TRUE(findSkipped(model, "struct_hostile_flexible") != nullptr);
		// A record is unsupported when any of its fields is.
		ASSERT_TRUE(findSkipped(model, "struct_hostile_has_union") != nullptr);

		ASSERT_TRUE(findSkipped(model, "hostile_printf") != nullptr);
		ASSERT_TRUE(findSkipped(model, "hostile_inline") != nullptr);
		ASSERT_TRUE(findSkipped(model, "hostile_int128") != nullptr);
		ASSERT_TRUE(findSkipped(model, "hostile_long_double") != nullptr);

		// A keyword cannot be renamed without breaking the link, so it is skipped.
		ASSERT_TRUE(findSkipped(model, "match") != nullptr);
		ASSERT_TRUE(findFunction(model, "match") == nullptr);

		// A function pointer is an opaque address; the declaration stays usable.
		const auto* callback = findFunction(model, "hostile_callback");
		ASSERT_TRUE(callback != nullptr);
		ASSERT_EQUAL(std::string("cptr u8"), renderParam(*callback, 0));

		const auto* void_pointer = findFunction(model, "hostile_void_pointer");
		ASSERT_TRUE(void_pointer != nullptr);
		ASSERT_EQUAL(std::string("cptr u8"), renderParam(*void_pointer, 0));
		ASSERT_EQUAL(std::string("cptr u8"), renderReturn(*void_pointer));

		// Dropping an anonymous member would leave a class of the wrong size with no error at
		// all, so the whole record has to go.
		ASSERT_TRUE(findSkipped(model, "struct_hostile_anon_member") != nullptr);
		ASSERT_TRUE(findRecord(model, "struct_hostile_anon_member") == nullptr);

		// A nested definition is not a top-level cursor, but it still needs a class.
		ASSERT_TRUE(findRecord(model, "struct_hostile_inner") != nullptr);
		ASSERT_EQUAL(
			std::string("struct_hostile_inner"),
			renderType(findRecord(model, "struct_hostile_outer")->fields.at(0).type)
		);

		// An unnamed record used as a named field gets a class of its own too.
		const auto* named_anon = findRecord(model, "struct_hostile_named_anon");
		ASSERT_TRUE(named_anon != nullptr);
		ASSERT_TRUE(findRecord(model, renderType(named_anon->fields.at(0).type)) != nullptr);

		// `hostile_holder` is declared before `hostile_later` is known to be unsupported, so the
		// pointer has to degrade rather than name a class that is never emitted.
		ASSERT_EQUAL(
			std::string("cptr u8"),
			renderType(findRecord(model, "struct_hostile_holder")->fields.at(0).type)
		);

		// A field name is positional in the C ABI, so a keyword is renamed instead of costing
		// the whole record.
		const auto* keyword_field = findRecord(model, "struct_hostile_keyword_field");
		ASSERT_TRUE(keyword_field != nullptr);
		ASSERT_EQUAL(std::string("type_"), keyword_field->fields.at(0).name);
		ASSERT_EQUAL(std::string("normal"), keyword_field->fields.at(1).name);

		// A keyword parameter name is replaced positionally; the function survives.
		const auto* keyword_param = findFunction(model, "hostile_keyword_param");
		ASSERT_TRUE(keyword_param != nullptr);
		ASSERT_EQUAL(std::string("arg0"), keyword_param->params.at(0).name);
	}

	void determinismTest() {
		const EmitterInput first{ .model      = read("headers/records.h"),
			                      .headers    = { "records.h" },
			                      .clang_args = {} };
		const EmitterInput second{ .model      = read("headers/records.h"),
			                       .headers    = { "records.h" },
			                       .clang_args = {} };
		ASSERT_EQUAL(emitBindings(first), emitBindings(second));
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/")

#include <c_import/tu_reader.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <string>

using namespace c_import;

namespace {

	ReadResult readFixture(const std::string& header) {
		ReadOptions options;
		options.headers      = { std::string{ C_IMPORT_TEST_HEADERS } + "/" + header };
		options.resource_dir = DUCK_CLANG_RESOURCE_DIR;
		return readHeaders(options);
	}

	const CRecord* findRecord(const CModel& model, std::string_view name) {
		auto it = std::ranges::find(model.records, name, &CRecord::name);
		return it == model.records.end() ? nullptr : &*it;
	}

	const CFunction* findFunction(const CModel& model, std::string_view name) {
		auto it = std::ranges::find(model.functions, name, &CFunction::name);
		return it == model.functions.end() ? nullptr : &*it;
	}

	const CConstant* findConstant(const CModel& model, std::string_view name) {
		auto it = std::ranges::find(model.constants, name, &CConstant::name);
		return it == model.constants.end() ? nullptr : &*it;
	}

}

class CImportReaderTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportReaderTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(compilesWithoutErrors);
		TESTER_ADD_TEST(recordsHaveClangLayouts);
		TESTER_ADD_TEST(forwardDeclaredRecordIsIncomplete);
		TESTER_ADD_TEST(anonymousMembersBecomeUnnamedFields);
		TESTER_ADD_TEST(bitfieldsKeepTheirWidth);
		TESTER_ADD_TEST(macrosTakeClangsTypeAndValue);
		TESTER_ADD_TEST(macrosWithoutANumberAreNotConstants);
		TESTER_ADD_TEST(enumeratorsAreRead);
		TESTER_ADD_TEST(functionsAreRead);
		TESTER_ADD_TEST(onlyTheRequestedHeadersAreRequested);
	}

private:
	ReadResult result = readFixture("sdl_like.h");

	void compilesWithoutErrors() {
		assertTrue(
			result.errors.empty(),
			"the fixture should compile: " + (result.errors.empty() ? "" : result.errors[0])
		);
	}

	void recordsHaveClangLayouts() {
		const auto* rect = findRecord(result.model, "SL_Rect");
		assertTrue(rect != nullptr && rect->complete, "SL_Rect should be complete");
		assertEqual(std::uint64_t{ 16 }, rect->size, "SL_Rect size");
		assertEqual(std::uint64_t{ 4 }, rect->align, "SL_Rect align");
		assertEqual(std::size_t{ 4 }, rect->fields.size(), "SL_Rect fields");
		assertEqual(std::uint64_t{ 96 }, rect->fields[3].offset_bits, "SL_Rect.h offset");

		const auto* event = findRecord(result.model, "SL_Event");
		assertTrue(event != nullptr && event->is_union, "SL_Event should be a union");
		assertEqual(std::uint64_t{ 64 }, event->size, "SL_Event size");
		assertEqual(std::uint64_t{ 8 }, event->align, "SL_Event align");

		const auto* packed = findRecord(result.model, "SL_Packed");
		assertEqual(std::uint64_t{ 5 }, packed->size, "SL_Packed size");
		assertEqual(std::uint64_t{ 8 }, packed->fields[1].offset_bits, "SL_Packed.value offset");
	}

	void forwardDeclaredRecordIsIncomplete() {
		const auto* window = findRecord(result.model, "SL_Window");
		assertTrue(window != nullptr && !window->complete, "SL_Window should be incomplete");
	}

	void anonymousMembersBecomeUnnamedFields() {
		const auto* tagged = findRecord(result.model, "SL_Tagged");
		assertEqual(std::size_t{ 3 }, tagged->fields.size(), "SL_Tagged fields");
		const auto& anon_union = tagged->fields[1];
		assertTrue(anon_union.name.empty(), "anonymous union has no name");
		assertEqual(std::uint64_t{ 32 }, anon_union.offset_bits, "anonymous union offset");
		const auto& inner_union
			= result.model.records[std::get<CRecordRef>(anon_union.type->kind).index];
		assertTrue(
			inner_union.anonymous && inner_union.is_union, "inner record is an anonymous union"
		);

		const auto& anon_struct = tagged->fields[2];
		assertEqual(std::uint64_t{ 64 }, anon_struct.offset_bits, "anonymous struct offset");
	}

	void bitfieldsKeepTheirWidth() {
		const auto* flags = findRecord(result.model, "SL_Flags");
		assertEqual(std::uint32_t{ 3 }, flags->fields[1].bit_width.value_or(0), "mode width");
		assertEqual(std::uint64_t{ 1 }, flags->fields[1].offset_bits, "mode offset");
		assertEqual(std::uint64_t{ 8 }, flags->fields[3].offset_bits, "wide offset");
	}

	void macrosTakeClangsTypeAndValue() {
		const auto* video = findConstant(result.model, "SL_INIT_VIDEO");
		assertTrue(video != nullptr, "SL_INIT_VIDEO should be a constant");
		assertTrue(
			video->type == CScalar{ .kind = ScalarKind::UnsignedInt, .bits = 32 },
			"SL_INIT_VIDEO is unsigned int"
		);
		assertEqual(
			std::uint64_t{ 0x20 }, std::get<std::uint64_t>(video->value), "SL_INIT_VIDEO value"
		);

		const auto* resizable = findConstant(result.model, "SL_WINDOW_RESIZABLE");
		assertTrue(resizable != nullptr, "SL_WINDOW_RESIZABLE should be a constant");
		assertTrue(
			resizable->type == CScalar{ .kind = ScalarKind::UnsignedInt, .bits = 64 },
			"SL_WINDOW_RESIZABLE is 64-bit"
		);

		const auto* scale = findConstant(result.model, "SL_SCALE");
		assertTrue(
			scale != nullptr && scale->type == CScalar{ .kind = ScalarKind::Float, .bits = 64 },
			"SL_SCALE is a double"
		);
		assertTrue(std::get<double>(scale->value) == 1.5, "SL_SCALE value");

		const auto* negative = findConstant(result.model, "SL_NEGATIVE");
		assertEqual(
			std::int64_t{ -3 }, std::get<std::int64_t>(negative->value), "SL_NEGATIVE value"
		);
	}

	void macrosWithoutANumberAreNotConstants() {
		assertTrue(
			findConstant(result.model, "SL_EMPTY") == nullptr, "an empty macro is no constant"
		);
		assertTrue(
			findConstant(result.model, "SL_STRING") == nullptr, "a string macro is no constant"
		);
		assertTrue(
			findConstant(result.model, "SL_LIKE_H") == nullptr, "an include guard is no constant"
		);
		assertTrue(result.non_numeric_macros >= 2, "non-numeric macros are counted");
	}

	void enumeratorsAreRead() {
		auto it
			= std::ranges::find(result.model.enums, std::string{ "SL_EventType" }, &CEnum::name);
		assertTrue(it != result.model.enums.end(), "SL_EventType should be read");
		assertEqual(std::size_t{ 3 }, it->enumerators.size(), "SL_EventType enumerators");
		assertEqual(std::string{ "SL_EVENT_QUIT" }, it->enumerators[1].name, "second enumerator");
	}

	void functionsAreRead() {
		const auto* log = findFunction(result.model, "SL_Log");
		assertTrue(log != nullptr && log->variadic, "SL_Log is variadic");
		const auto* inline_fn = findFunction(result.model, "SL_Inline");
		assertTrue(
			inline_fn != nullptr && inline_fn->internal_linkage, "SL_Inline has internal linkage"
		);
		const auto* create = findFunction(result.model, "SL_CreateWindow");
		assertEqual(std::size_t{ 4 }, create->params.size(), "SL_CreateWindow params");
		assertTrue(
			std::holds_alternative<CPointer>(create->return_type->kind),
			"SL_CreateWindow returns a pointer"
		);
	}

	void onlyTheRequestedHeadersAreRequested() {
		const auto* init = findFunction(result.model, "SL_Init");
		assertTrue(init->location.in_requested_headers, "SL_Init comes from the requested header");
		auto from_stdint = std::ranges::find_if(result.model.records, [](const CRecord& record) {
			return record.location.file.ends_with("stdint.h");
		});
		assertTrue(
			from_stdint == result.model.records.end() || !from_stdint->location.in_requested_headers,
			"system headers are not requested"
		);
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/")

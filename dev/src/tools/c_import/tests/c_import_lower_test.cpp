#include <c_import/emit.hpp>
#include <c_import/identifiers.hpp>
#include <c_import/lower.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <string>

using namespace c_import;

namespace {

	const CLocation REQUESTED{ .file = "lib.h", .in_requested_headers = true };

	CTypeRef cI32() { return makeType(CScalar{ .kind = ScalarKind::SignedInt, .bits = 32 }); }

	CTypeRef cU32() { return makeType(CScalar{ .kind = ScalarKind::UnsignedInt, .bits = 32 }); }

	CTypeRef cF32() { return makeType(CScalar{ .kind = ScalarKind::Float, .bits = 32 }); }

	CTypeRef ptr(CTypeRef pointee) { return makeType(CPointer{ std::move(pointee) }); }

	CTypeRef rec(std::size_t index) { return makeType(CRecordRef{ index }); }

	CField field(std::string name, CTypeRef type, std::uint64_t offset_bytes, std::uint64_t size) {
		return CField{ .name        = std::move(name),
			           .type        = std::move(type),
			           .offset_bits = offset_bytes * 8,
			           .bit_width   = std::nullopt,
			           .size        = size,
			           .align       = size };
	}

	CRecord record(
		std::string name, std::vector<CField> fields, std::uint64_t size, std::uint64_t align
	) {
		CRecord result;
		result.name     = std::move(name);
		result.complete = true;
		result.size     = size;
		result.align    = align;
		result.fields   = std::move(fields);
		result.location = REQUESTED;
		return result;
	}

	CFunction function(std::string name, CTypeRef ret, std::vector<CParam> params) {
		CFunction result;
		result.name        = std::move(name);
		result.return_type = std::move(ret);
		result.params      = std::move(params);
		result.location    = REQUESTED;
		return result;
	}

	template<typename T>
	const T* named(const std::vector<T>& items, std::string_view name) {
		auto it = std::ranges::find(items, name, &T::name);
		return it == items.end() ? nullptr : &*it;
	}

	bool isSkipped(const DkModule& module, std::string_view name) {
		return named(module.skipped, name) != nullptr;
	}

}

class CImportLowerTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportLowerTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(naturalStructBecomesClass);
		TESTER_ADD_TEST(arrayOfPointersIsParenthesized);
		TESTER_ADD_TEST(packedStructBecomesBlobWithByteAccessors);
		TESTER_ADD_TEST(unionBecomesBlobWithPointerViews);
		TESTER_ADD_TEST(anonymousUnionMemberGetsAccessorsOnOuter);
		TESTER_ADD_TEST(incompleteRecordIsOpaque);
		TESTER_ADD_TEST(completeRecordBehindPointerKeepsLayout);
		TESTER_ADD_TEST(blobWithFloatByValueIsSkipped);
		TESTER_ADD_TEST(unlinkableFunctionsAreSkipped);
		TESTER_ADD_TEST(recordNameClashingWithFunctionIsRenamed);
		TESTER_ADD_TEST(filtersSelectDeclarations);
		TESTER_ADD_TEST(constantLiterals);
		TESTER_ADD_TEST(emittedModuleShape);
		TESTER_ADD_TEST(globs);
	}

private:
	void naturalStructBecomesClass() {
		CModel model;
		model.records.push_back(
			record("Rect", { field("x", cI32(), 0, 4), field("type", cI32(), 4, 4) }, 8, 4)
		);
		auto        module = lower(model, {});
		const auto* cls    = named(module.classes, "Rect");
		assertTrue(cls != nullptr, "Rect should be a class");
		assertEqual(std::size_t{ 2 }, cls->fields.size(), "Rect fields");
		assertEqual(std::string{ "type_" }, cls->fields[1].name, "a keyword field is renamed");
		assertEqual(std::size_t{ 1 }, module.layouts.size(), "Rect's layout is checked");
	}

	void arrayOfPointersIsParenthesized() {
		CModel model;
		auto   names = makeType(CArray{ .element = ptr(cI32()), .count = 13 });
		CField field{ .name        = "names",
			          .type        = names,
			          .offset_bits = 0,
			          .bit_width   = std::nullopt,
			          .size        = 104,
			          .align       = 8 };
		model.records.push_back(record("Locale", { field }, 104, 8));
		auto module = lower(model, {});
		assertEqual(
			std::string{ "(cptr i32)[13]" },
			named(module.classes, "Locale")->fields.at(0).type,
			"an array of pointers, not a pointer to an array"
		);
	}

	void packedStructBecomesBlobWithByteAccessors() {
		CModel model;
		auto   u8 = makeType(CScalar{ .kind = ScalarKind::UnsignedInt, .bits = 8 });
		model.records.push_back(
			record("Packed", { field("tag", u8, 0, 1), field("value", cU32(), 1, 4) }, 5, 1)
		);
		auto        module = lower(model, {});
		const auto* cls    = named(module.classes, "Packed");
		assertEqual(
			std::string{ "u8[5]" }, cls->fields.at(0).type, "storage matches size and alignment"
		);
		assertTrue(
			named(module.functions, "Packed_get_value") != nullptr, "misaligned field getter"
		);
		assertTrue(
			named(module.functions, "Packed_set_value") != nullptr, "misaligned field setter"
		);
		assertTrue(named(module.functions, "Packed_get_tag") != nullptr, "aligned field getter");
	}

	void unionBecomesBlobWithPointerViews() {
		CModel model;
		model.records.push_back(
			record("Key", { field("type", cU32(), 0, 4), field("code", cI32(), 4, 4) }, 8, 4)
		);
		auto ev
			= record("Event", { field("type", cU32(), 0, 4), field("key", rec(0), 0, 8) }, 8, 4);
		ev.is_union = true;
		model.records.push_back(ev);
		auto module = lower(model, {});
		assertEqual(
			std::string{ "u32[2]" },
			named(module.classes, "Event")->fields.at(0).type,
			"union storage"
		);
		const auto* view = named(module.functions, "Event_as_key");
		assertTrue(view != nullptr, "union member view");
		assertEqual(std::string{ "cptr Key" }, view->return_type, "view type");
		assertEqual(
			std::string{ "cptr u32" },
			named(module.functions, "Event_as_type")->return_type,
			"scalar view type"
		);
	}

	void anonymousUnionMemberGetsAccessorsOnOuter() {
		CModel model;
		auto   inner    = record("", { field("i", cI32(), 0, 4), field("f", cF32(), 0, 4) }, 4, 4);
		inner.is_union  = true;
		inner.anonymous = true;
		model.records.push_back(inner);
		model.records.push_back(
			record("Tagged", { field("kind", cI32(), 0, 4), field("", rec(0), 4, 4) }, 8, 4)
		);
		auto        module = lower(model, {});
		const auto* cls    = named(module.classes, "Tagged");
		assertTrue(cls != nullptr && cls->fields.size() == 2, "Tagged keeps its natural layout");
		assertEqual(
			std::string{ "Tagged__anon0" },
			cls->fields[1].type,
			"anonymous union is a named blob field"
		);
		assertTrue(
			named(module.functions, "Tagged_get_f") != nullptr,
			"alternative getter on the outer record"
		);
		assertTrue(named(module.classes, "Tagged__view_f") != nullptr, "offset view class");
	}

	void incompleteRecordIsOpaque() {
		CModel  model;
		CRecord window;
		window.name     = "Window";
		window.location = REQUESTED;
		model.records.push_back(window);
		model.functions.push_back(
			function("destroy_window", makeType(CVoid{}), { { .name = "w", .type = ptr(rec(0)) } })
		);
		auto        module = lower(model, {});
		const auto* cls    = named(module.classes, "Window");
		assertTrue(
			cls != nullptr && cls->fields.at(0).name == "_opaque",
			"incomplete record is an opaque handle"
		);
		assertEqual(
			std::string{ "cptr Window" },
			module.fundecls.at(0).params.at(0).type,
			"typed handle pointer"
		);
		assertTrue(!module.fundecls.at(0).return_type.has_value(), "void return has no type");
		assertTrue(module.layouts.empty(), "an opaque handle has no layout to check");
	}

	void completeRecordBehindPointerKeepsLayout() {
		CModel  model;
		CRecord mpz
			= record("mpz", { field("alloc", cI32(), 0, 4), field("size", cI32(), 4, 4) }, 8, 4);
		mpz.location.in_requested_headers = false;
		model.records.push_back(mpz);
		model.functions.push_back(
			function("mpz_init", makeType(CVoid{}), { { .name = "z", .type = ptr(rec(0)) } })
		);
		auto        module = lower(model, { .include = { "mpz_init" }, .exclude = {} });
		const auto* cls    = named(module.classes, "mpz");
		assertTrue(
			cls != nullptr && cls->fields.size() == 2,
			"a complete record keeps its fields, so it can be allocated"
		);
	}

	void blobWithFloatByValueIsSkipped() {
		CModel model;
		auto   u   = record("U", { field("i", cI32(), 0, 4), field("f", cF32(), 0, 4) }, 4, 4);
		u.is_union = true;
		model.records.push_back(u);
		model.functions.push_back(
			function("take", makeType(CVoid{}), { { .name = "u", .type = rec(0) } })
		);
		model.functions.push_back(
			function("take_ptr", makeType(CVoid{}), { { .name = "u", .type = ptr(rec(0)) } })
		);
		auto module = lower(model, {});
		assertTrue(
			isSkipped(module, "take"), "a small blob with a float member is not passed by value"
		);
		assertTrue(named(module.fundecls, "take_ptr") != nullptr, "a pointer to it is fine");
	}

	void unlinkableFunctionsAreSkipped() {
		CModel model;
		auto   variadic           = function("log", makeType(CVoid{}), {});
		variadic.variadic         = true;
		auto internal             = function("helper", cI32(), {});
		internal.internal_linkage = true;
		model.functions           = { variadic,
			                          internal,
			                          function("match", cI32(), { { .name = "in", .type = cI32() } }),
			                          function("ok", cI32(), { { .name = "in", .type = cI32() } }) };
		auto module               = lower(model, {});
		assertTrue(isSkipped(module, "log"), "variadic");
		assertTrue(isSkipped(module, "helper"), "static");
		assertTrue(isSkipped(module, "match"), "keyword-named function");
		assertEqual(
			std::string{ "in_" },
			named(module.fundecls, "ok")->params.at(0).name,
			"keyword parameter renamed"
		);
	}

	void recordNameClashingWithFunctionIsRenamed() {
		CModel model;
		model.records.push_back(record("stat", { field("size", cI32(), 0, 4) }, 4, 4));
		model.functions.push_back(
			function("stat", cI32(), { { .name = "buf", .type = ptr(rec(0)) } })
		);
		auto module = lower(model, {});
		assertTrue(named(module.classes, "stat_struct") != nullptr, "record renamed");
		assertEqual(
			std::string{ "cptr stat_struct" },
			named(module.fundecls, "stat")->params.at(0).type,
			"uses the new name"
		);
	}

	void filtersSelectDeclarations() {
		CModel model;
		model.functions = { function("SL_Init", cI32(), {}),
			                function("SL_main", cI32(), {}),
			                function("other", cI32(), {}) };
		auto module     = lower(model, { .include = { "SL_*" }, .exclude = { "SL_main" } });
		assertEqual(std::size_t{ 1 }, module.fundecls.size(), "only SL_Init passes");
		assertEqual(std::string{ "SL_Init" }, module.fundecls[0].name, "SL_Init");
	}

	void constantLiterals() {
		CModel model;
		model.constants = {
			{ .name     = "A",
			  .type     = { .kind = ScalarKind::UnsignedInt, .bits = 32 },
			  .value    = std::uint64_t{ 32 },
			  .location = REQUESTED },
			{ .name     = "B",
			  .type     = { .kind = ScalarKind::SignedInt, .bits = 32 },
			  .value    = std::int64_t{ -3 },
			  .location = REQUESTED },
			{ .name     = "C",
			  .type     = { .kind = ScalarKind::SignedInt, .bits = 64 },
			  .value    = std::numeric_limits<std::int64_t>::min(),
			  .location = REQUESTED },
			{ .name     = "D",
			  .type     = { .kind = ScalarKind::Float, .bits = 64 },
			  .value    = 1.5,
			  .location = REQUESTED },
			{ .name     = "E",
			  .type     = { .kind = ScalarKind::Float, .bits = 32 },
			  .value    = 2.0,
			  .location = REQUESTED },
			{ .name     = "F",
			  .type     = { .kind = ScalarKind::Bool, .bits = 8 },
			  .value    = std::uint64_t{ 1 },
			  .location = REQUESTED },
		};
		auto module = lower(model, {});
		assertEqual(std::string{ "32u32" }, named(module.constants, "A")->value, "unsigned");
		assertEqual(std::string{ "-3i32" }, named(module.constants, "B")->value, "negative");
		assertEqual(
			std::string{ "(-9223372036854775807i64 - 1i64)" },
			named(module.constants, "C")->value,
			"minimum"
		);
		assertEqual(std::string{ "1.5" }, named(module.constants, "D")->value, "double");
		assertEqual(std::string{ "2.0f32" }, named(module.constants, "E")->value, "float");
		assertEqual(std::string{ "true" }, named(module.constants, "F")->value, "bool");
	}

	void emittedModuleShape() {
		CModel model;
		model.records.push_back(record("Rect", { field("x", cI32(), 0, 4) }, 4, 4));
		model.functions.push_back(function("area", cI32(), { { .name = "r", .type = rec(0) } }));
		auto variadic     = function("log", makeType(CVoid{}), {});
		variadic.variadic = true;
		model.functions.push_back(variadic);
		auto module = lower(model, {});
		auto text   = emitModule(module, { "generated" });
		assertEqual(
			std::string{ "# generated\n"
		                 "\n"
		                 "# Skipped declarations:\n"
		                 "#   log: variadic function (#3271)\n"
		                 "\n"
		                 "extern(\"C\") {\n"
		                 "    class Rect {\n"
		                 "        x: i32;\n"
		                 "    }\n"
		                 "\n"
		                 "    fundecl area(r: Rect) -> i32;\n"
		                 "}\n" },
			text,
			"module text"
		);
		auto check = emitLayoutCheck(module, {}, "lib.generated.lib");
		assertTrue(check.contains("const C_LAYOUT_SIZE_0: i64 = size_of(Rect);"), "size constant");
		assertTrue(check.contains("if (C_LAYOUT_SIZE_0 != 4i64) { return 1i64; }"), "size check");
	}

	void globs() {
		assertTrue(globMatch("SL_*", "SL_Init"), "prefix");
		assertTrue(globMatch("*Event", "SL_Event"), "suffix");
		assertTrue(globMatch("S?_*t", "SL_Event"), "mixed");
		assertTrue(!globMatch("SL_*", "XSL_Init"), "anchored");
		assertTrue(
			isReservedName("match") && isReservedName("u8") && !isReservedName("SL_Init"),
			"reserved names"
		);
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/")

#include <abi/layout/compute_c_layout.hpp>
#include <abi/target.hpp>
#include <abi/type_system/type.hpp>

#include <base/except/exceptions.hpp>

#include <tester/tester.hpp>

#include <vector>

namespace at = abi::types;
namespace al = abi::layout;

namespace {

	template<class... Ts>
	std::vector<at::AbiTypePtr> fieldsOf(Ts&&... types) {
		std::vector<at::AbiTypePtr> out;
		out.reserve(sizeof...(Ts));
		(out.push_back(at::makeBoxAbiType(std::forward<Ts>(types))), ...);
		return out;
	}

}

class AbiLayoutTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AbiLayoutTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(presetsTest);
		TESTER_ADD_TEST(singleI8Test);
		TESTER_ADD_TEST(i8ThenI32Test);
		TESTER_ADD_TEST(i32ThenI8Test);
		TESTER_ADD_TEST(i8ThenI64Test);
		TESTER_ADD_TEST(i8ThenPointerTest);
		TESTER_ADD_TEST(pointerThenI8Test);
		TESTER_ADD_TEST(singleFloatTest);
		TESTER_ADD_TEST(floatThenDoubleTest);
		TESTER_ADD_TEST(i8ThenDoubleTest);
		TESTER_ADD_TEST(quadFloatTest);
		TESTER_ADD_TEST(x87LongDoubleTest);
		TESTER_ADD_TEST(i8ThenLongDoubleTest);
		TESTER_ADD_TEST(singleBoolTest);
		TESTER_ADD_TEST(boolCharThenI16Test);
		TESTER_ADD_TEST(arrayOfI32Test);
		TESTER_ADD_TEST(i8ThenArrayOfI32Test);
		TESTER_ADD_TEST(nestedStructTest);
		TESTER_ADD_TEST(threePointersTest);
		TESTER_ADD_TEST(aarch64MatchesX86Test);
		TESTER_ADD_TEST(rejectsZeroLengthArrayTest);
		TESTER_ADD_TEST(rejectsEmptyStructTest);
		TESTER_ADD_TEST(rejectsEmptyFieldListTest);
	}

private:
	void presetsTest() {
		abi::TargetABI x = abi::x86_64Linux();
		assertTrue(x.triple.arch == abi::Arch::X86_64, "x86_64 arch");
		assertTrue(usize(x.data_layout.pointer_size) == 8, "pointer size on x86_64");
		assertTrue(usize(x.data_layout.pointer_alignment) == 8, "pointer align on x86_64");

		abi::TargetABI a = abi::aarch64Linux();
		assertTrue(a.triple.arch == abi::Arch::AArch64, "aarch64 arch");
		assertTrue(usize(a.data_layout.pointer_size) == 8, "pointer size on aarch64");
		assertTrue(usize(a.data_layout.pointer_alignment) == 8, "pointer align on aarch64");

		// f80 (x87 long double) is present on x86_64 (16/16) but absent on
		// aarch64; f128 (IEEE quad) is present on both.
		const auto x_f80 = x.data_layout.float_layouts.atMaybeCopy(u64(80));
		assertTrue(x_f80.has_value(), "x86_64 has an f80 entry");
		assertTrue(usize(x_f80.value().size) == 16, "x86_64 f80 size");
		assertTrue(usize(x_f80.value().alignment) == 16, "x86_64 f80 align");
		assertTrue(
			!a.data_layout.float_layouts.atMaybeCopy(u64(80)).has_value(), "aarch64 has no f80"
		);
		assertTrue(x.data_layout.float_layouts.atMaybeCopy(u64(128)).has_value(), "x86_64 has f128");
		assertTrue(
			a.data_layout.float_layouts.atMaybeCopy(u64(128)).has_value(), "aarch64 has f128"
		);
	}

	static void expectLayout(
		al::ComputedLayout layout, usize size, usize align, std::vector<usize> offsets
	) {
		if (usize(layout.size) != size)
			CORE_PANIC("size mismatch: expected ", size, " got ", usize(layout.size));
		if (usize(layout.alignment) != align)
			CORE_PANIC("align mismatch: expected ", align, " got ", usize(layout.alignment));
		if (layout.field_offsets.size() != offsets.size()) CORE_PANIC("offsets count mismatch");
		for (usize i = 0; i < offsets.size(); ++i)
			if (usize(layout.field_offsets[i]) != offsets[i])
				CORE_PANIC(
					"offset[",
					i,
					"] mismatch: expected ",
					offsets[i],
					" got ",
					usize(layout.field_offsets[i])
				);
	}

	void singleI8Test() {
		auto layout = al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::intType(8, true)));
		expectLayout(std::move(layout), 1, 1, { 0 });
	}

	void i8ThenI32Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(8, true), at::intType(32, true))
		);
		expectLayout(std::move(layout), 8, 4, { 0, 4 });
	}

	void i32ThenI8Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(32, true), at::intType(8, true))
		);
		expectLayout(std::move(layout), 8, 4, { 0, 4 });
	}

	void i8ThenI64Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(8, true), at::intType(64, true))
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void i8ThenPointerTest() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(8, true), at::pointerType())
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void pointerThenI8Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::pointerType(), at::intType(8, true))
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void singleFloatTest() {
		auto layout = al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::floatType(32)));
		expectLayout(std::move(layout), 4, 4, { 0 });
	}

	void floatThenDoubleTest() {
		// float@0..4, pad 4..8, double@8..16 → total 16, align 8.
		auto layout
			= al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::floatType(32), at::floatType(64)));
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void i8ThenDoubleTest() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(8, true), at::floatType(64))
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void quadFloatTest() {
		// f128 (IEEE binary128) is 16/16 and identical across targets.
		auto x86 = al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::floatType(128)));
		expectLayout(std::move(x86), 16, 16, { 0 });
		auto arm = al::computeCLayout(abi::aarch64Linux(), fieldsOf(at::floatType(128)));
		expectLayout(std::move(arm), 16, 16, { 0 });
	}

	void x87LongDoubleTest() {
		// f80 on x86_64: 80 bits of data stored in 16 bytes, align 16.
		auto layout = al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::floatType(80)));
		expectLayout(std::move(layout), 16, 16, { 0 });
	}

	void i8ThenLongDoubleTest() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::intType(8, true), at::floatType(80))
		);
		expectLayout(std::move(layout), 32, 16, { 0, 16 });
	}

	void singleBoolTest() {
		// C `_Bool` is a 1-byte type.
		auto layout = al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::boolType()));
		expectLayout(std::move(layout), 1, 1, { 0 });
	}

	void boolCharThenI16Test() {
		// bool@0 (1/1), char@1 (1/1), i16 align 2 → @2..4. Total 4, align 2.
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::boolType(), at::charType(), at::intType(16, true))
		);
		expectLayout(std::move(layout), 4, 2, { 0, 1, 2 });
	}

	void arrayOfI32Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::arrayType(at::makeBoxAbiType(at::intType(32, true)), 3))
		);
		expectLayout(std::move(layout), 12, 4, { 0 });
	}

	void i8ThenArrayOfI32Test() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(),
			fieldsOf(
				at::intType(8, true), at::arrayType(at::makeBoxAbiType(at::intType(32, true)), 3)
			)
		);
		expectLayout(std::move(layout), 16, 4, { 0, 4 });
	}

	void nestedStructTest() {
		at::AbiType inner  = at::structType(fieldsOf(at::intType(8, true), at::intType(64, true)));
		auto        layout = al::computeCLayout(
            abi::x86_64Linux(),
            fieldsOf(at::intType(32, true), std::move(inner), at::intType(8, true))
        );
		// Layout: i32@0..4, pad 4..8, inner{i8,i64}@8..24, i8@24..25, pad 25..32.
		expectLayout(std::move(layout), 32, 8, { 0, 8, 24 });
	}

	void threePointersTest() {
		auto layout = al::computeCLayout(
			abi::x86_64Linux(), fieldsOf(at::pointerType(), at::pointerType(), at::pointerType())
		);
		expectLayout(std::move(layout), 24, 8, { 0, 8, 16 });
	}

	void aarch64MatchesX86Test() {
		auto x86 = al::computeCLayout(
			abi::x86_64Linux(),
			fieldsOf(at::intType(8, true), at::pointerType(), at::intType(32, true))
		);
		auto arm = al::computeCLayout(
			abi::aarch64Linux(),
			fieldsOf(at::intType(8, true), at::pointerType(), at::intType(32, true))
		);
		assertTrue(usize(x86.size) == usize(arm.size), "size matches across targets");
		assertTrue(usize(x86.alignment) == usize(arm.alignment), "align matches across targets");
		for (usize i = 0; i < x86.field_offsets.size(); ++i)
			assertTrue(
				usize(x86.field_offsets[i]) == usize(arm.field_offsets[i]),
				"offset matches across targets"
			);
	}

	void rejectsZeroLengthArrayTest() {
		assertThrows<base::Panic>(
			[]() {
				(void) al::computeCLayout(
					abi::x86_64Linux(),
					fieldsOf(at::arrayType(at::makeBoxAbiType(at::intType(32, true)), 0))
				);
			},
			"zero-length array should panic"
		);
	}

	void rejectsEmptyStructTest() {
		assertThrows<base::Panic>(
			[]() { (void) al::computeCLayout(abi::x86_64Linux(), fieldsOf(at::structType({}))); },
			"empty nested struct should panic"
		);
	}

	void rejectsEmptyFieldListTest() {
		assertThrows<base::Panic>(
			[]() { (void) al::computeCLayout(abi::x86_64Linux(), {}); },
			"empty top-level field list should panic"
		);
	}
};

TESTER_COMMON_MAIN("/src/common/abi/layout/tests/");

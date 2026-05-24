#include <abi/layout/compute_c_layout.hpp>
#include <abi/layout/target.hpp>
#include <abi/type_system/type.hpp>

#include <base/except/exceptions.hpp>

#include <tester/tester.hpp>

#include <string>
#include <vector>

namespace at = abi::type_system;
namespace al = abi::layout;

namespace {

	at::Field anonField(at::AbiType type) {
		return at::field(base::Optional<std::string>{}, std::move(type));
	}

	template<class... Ts>
	std::vector<at::Field> fieldsOf(Ts&&... types) {
		std::vector<at::Field> out;
		out.reserve(sizeof...(Ts));
		(out.push_back(anonField(std::forward<Ts>(types))), ...);
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
		al::TargetABI x = al::x86_64Linux();
		assertTrue(x.triple.arch == al::Arch::X86_64, "x86_64 arch");
		assertTrue(usize(x.data_layout.pointer_size) == 8, "pointer size on x86_64");
		assertTrue(usize(x.data_layout.pointer_alignment) == 8, "pointer align on x86_64");

		al::TargetABI a = al::aarch64Linux();
		assertTrue(a.triple.arch == al::Arch::AArch64, "aarch64 arch");
		assertTrue(usize(a.data_layout.pointer_size) == 8, "pointer size on aarch64");
		assertTrue(usize(a.data_layout.pointer_alignment) == 8, "pointer align on aarch64");
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
		auto layout = al::computeCLayout(al::x86_64Linux(), fieldsOf(at::intType(u8(8), true)));
		expectLayout(std::move(layout), 1, 1, { 0 });
	}

	void i8ThenI32Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::intType(u8(8), true), at::intType(u8(32), true))
		);
		expectLayout(std::move(layout), 8, 4, { 0, 4 });
	}

	void i32ThenI8Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::intType(u8(32), true), at::intType(u8(8), true))
		);
		expectLayout(std::move(layout), 8, 4, { 0, 4 });
	}

	void i8ThenI64Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::intType(u8(8), true), at::intType(u8(64), true))
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void i8ThenPointerTest() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::intType(u8(8), true), at::pointerType())
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void pointerThenI8Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::pointerType(), at::intType(u8(8), true))
		);
		expectLayout(std::move(layout), 16, 8, { 0, 8 });
	}

	void arrayOfI32Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::arrayType(at::intType(u8(32), true), 3))
		);
		expectLayout(std::move(layout), 12, 4, { 0 });
	}

	void i8ThenArrayOfI32Test() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(),
			fieldsOf(at::intType(u8(8), true), at::arrayType(at::intType(u8(32), true), 3))
		);
		expectLayout(std::move(layout), 16, 4, { 0, 4 });
	}

	void nestedStructTest() {
		at::AbiType inner
			= at::structType(fieldsOf(at::intType(u8(8), true), at::intType(u8(64), true)));
		auto layout = al::computeCLayout(
			al::x86_64Linux(),
			fieldsOf(at::intType(u8(32), true), std::move(inner), at::intType(u8(8), true))
		);
		// Layout: i32@0..4, pad 4..8, inner{i8,i64}@8..24, i8@24..25, pad 25..32.
		expectLayout(std::move(layout), 32, 8, { 0, 8, 24 });
	}

	void threePointersTest() {
		auto layout = al::computeCLayout(
			al::x86_64Linux(), fieldsOf(at::pointerType(), at::pointerType(), at::pointerType())
		);
		expectLayout(std::move(layout), 24, 8, { 0, 8, 16 });
	}

	void aarch64MatchesX86Test() {
		auto x86 = al::computeCLayout(
			al::x86_64Linux(),
			fieldsOf(at::intType(u8(8), true), at::pointerType(), at::intType(u8(32), true))
		);
		auto arm = al::computeCLayout(
			al::aarch64Linux(),
			fieldsOf(at::intType(u8(8), true), at::pointerType(), at::intType(u8(32), true))
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
					al::x86_64Linux(), fieldsOf(at::arrayType(at::intType(u8(32), true), 0))
				);
			},
			"zero-length array should panic"
		);
	}

	void rejectsEmptyStructTest() {
		assertThrows<base::Panic>(
			[]() { (void) al::computeCLayout(al::x86_64Linux(), fieldsOf(at::structType({}))); },
			"empty nested struct should panic"
		);
	}

	void rejectsEmptyFieldListTest() {
		assertThrows<base::Panic>(
			[]() { (void) al::computeCLayout(al::x86_64Linux(), {}); },
			"empty top-level field list should panic"
		);
	}
};

TESTER_COMMON_MAIN("/src/common/abi/layout/tests/");

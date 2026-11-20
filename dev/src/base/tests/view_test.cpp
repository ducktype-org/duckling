// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/misc/raw_view.hpp>

#include <tester/tester.hpp>

#include <array>

class ViewTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ViewTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(rawViewTest);
		TESTER_ADD_TEST(owningViewTest);
		TESTER_ADD_TEST(typedModRawViewTest);
		TESTER_ADD_TEST(typedOwningViewTest);
	}

	void rawViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte* string = reinterpret_cast<const byte*>("Some random string");
		{ base::RawView view(string, 18); }
		base::RawView view(string, 18);

		assertTrue(view.stringView() == "Some random string", "bad RawView.stringView()");
		assertTrue(view.stdString() == "Some random string", "bad RawView.stdString()");
		assertTrue(view.getBegin() == string, "bad RawView.begin");
		assertTrue(view.size() == 18, "bad RawView.size");
	}

	void owningViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte* string_1 = reinterpret_cast<const byte*>("Some random string 1");
		const char* string_2 = "Some random string 2";

		{
			base::OwningView empty_view_1;
			base::OwningView empty_view_2(nullptr);
		}

		{
			base::OwningView copy_view(string_2);
			assertTrue(
				copy_view.view().getBegin() != reinterpret_cast<const byte*>(string_2),
				"Owning view didn't make memory copy (1)"
			);
			assertTrue(copy_view.view().stringView() == string_2, "Owning view has bad content (1)");

			auto copy_view_2 = base::OwningView::copy(base::RawView(string_1, 20));
			assertTrue(
				copy_view_2.view().getBegin() != reinterpret_cast<const byte*>(string_1),
				"Owning view didn't make memory copy (2)"
			);
			assertTrue(
				copy_view_2.view().stringView() == reinterpret_cast<const char*>(string_1),
				"Owning view has bad content (2)"
			);
		}
	}

	void typedModRawViewTest() {
		std::array<u32, 4> numbers{ 1, 2, 3, 4 };

		base::TypedModRawView<u32> view(numbers.data(), numbers.size());

		assertTrue(view.getBegin() == numbers.data(), "bad TypedModRawView.getBegin()");
		assertTrue(view.size() == 4, "bad TypedModRawView.size(), it counts entries not bytes");
		assertTrue(view[2] == 3, "bad TypedModRawView.operator[]");

		// `stringView` is the one place a byte length is wanted, so it must scale by sizeof(T).
		assertTrue(
			view.stringView().size() == 4 * sizeof(u32), "TypedModRawView.stringView() is not bytes"
		);

		auto sub = view.subview(1, 2);
		assertTrue(sub.size() == 2, "bad TypedModRawView.subview() size");
		assertTrue(sub[0] == 2 && sub[1] == 3, "bad TypedModRawView.subview() content");

		view[0] = 42;
		assertTrue(numbers[0] == 42, "TypedModRawView does not write through");
	}

	void typedOwningViewTest() {
		message("Parts of this test are relevant only under valgrind");

		{
			base::TypedOwningView<u32> empty_view_1;
			base::TypedOwningView<u32> empty_view_2(nullptr);
			assertTrue(empty_view_1.size() == 0, "default TypedOwningView is not empty");
			assertTrue(empty_view_2.getBegin() == nullptr, "nullptr TypedOwningView is not null");
		}

		{
			base::TypedOwningView<u32> owner(new u32[3]{ 7, 8, 9 }, 3);
			const u32*                 raw = owner.getBegin();

			assertTrue(owner.size() == 3, "bad TypedOwningView.size()");
			assertTrue(owner.modView()[1] == 8, "bad TypedOwningView.modView()");
			assertTrue(owner.view().size() == 3, "bad TypedOwningView.view()");

			// The move constructor delegates to move assignment, so pin both: the moved-to view
			// must adopt the buffer and the moved-from one must be emptied, not left aliasing it
			// (a double free otherwise).
			base::TypedOwningView<u32> moved(std::move(owner));

			assertTrue(moved.getBegin() == raw, "move ctor did not adopt the buffer");
			assertTrue(moved.size() == 3, "move ctor lost the size");

			// Inspecting the moved-from object is exactly what is under test here: it must have
			// been emptied rather than left aliasing the buffer, which would double-free. Reading
			// it is well-defined for this type, so the use-after-move check is suppressed.
			// NOLINTBEGIN(clang-analyzer-cplusplus.Move)
			assertTrue(owner.getBegin() == nullptr, "move ctor left the source aliasing the buffer");
			assertTrue(owner.size() == 0, "move ctor left a stale size on the source");

			base::TypedOwningView<u32> assigned;
			assigned = std::move(moved);

			assertTrue(assigned.getBegin() == raw, "move assign did not adopt the buffer");
			assertTrue(moved.getBegin() == nullptr, "move assign left the source aliasing");
			// NOLINTEND(clang-analyzer-cplusplus.Move)
		}
	}

	~ViewTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");

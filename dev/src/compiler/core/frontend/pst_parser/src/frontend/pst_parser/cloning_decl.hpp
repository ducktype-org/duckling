#pragma once

namespace pst {
	struct CloneDummy {
	private:
		constexpr CloneDummy() = default;
		friend constexpr CloneDummy makeCloneDummy();
	};

	constexpr CloneDummy makeCloneDummy() { return {}; }

	constexpr CloneDummy CLONE = makeCloneDummy();
}

#define PARENT_CLASS(parent_class) private: using ParentClass = parent_class

#define ELEMENT_CLONE_DECL(element) \
	protected: \
		explicit element(pst::CloneDummy clone, element& other): ParentClass(clone, other) {}

#define CLONE_SIGNATURE(element_type) MBox<element_type> cloneElement() const
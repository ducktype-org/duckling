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
	explicit element(const pst::CloneDummy clone, const element& other): ParentClass(clone, other) {}

#define CLONE_SIGNATURE(element_type) MBox<LangElement> cloneElement() const
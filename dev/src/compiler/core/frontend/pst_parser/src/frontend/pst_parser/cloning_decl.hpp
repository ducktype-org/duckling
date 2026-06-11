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

#define THIS_CLASS(this_class) private: using ThisClass = this_class
#define PARENT_CLASS(parent_class) private: using ParentClass = parent_class

#define CONSTRUCTOR_VALUE_COPY(member) , member(other.member)

#define ELEMENT_CLONE_DECL(element, ...) \
	explicit element(const pst::CloneDummy clone, const element& other): ParentClass(clone, other) FOR_EACH(CONSTRUCTOR_VALUE_COPY, __VA_ARGS__) {}

#define CLONE_SIGNATURE() MBox<LangElement> cloneElement() const

#define CLONE_SIGNATURE_DEFAULT_OVERRIDE() MBox<LangElement> cloneElement() const override {\
	static_assert(std::is_final_v<ThisClass> == true);\
	auto out =  base::makeBox<ThisClass>(pst::makeCloneDummy(), *this);\
	out->cloneSubElements(*this);\
	return out;\
}

#define CLONE_SUBELEMENTS() protected: void cloneSubElements(const ThisClass&)

#define SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(element, parent_class, ...) \
	public:\
		ELEMENT_CLONE_DECL(element, __VA_ARGS__);\
	private:\
		THIS_CLASS(element);\
		PARENT_CLASS(parent_class);\
		CLONE_SIGNATURE_DEFAULT_OVERRIDE()
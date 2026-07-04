#pragma once

namespace pst {
	/**
	 * @brief Dummy class used to make constructor used for cloning more explicit and to not
	 * accidentally use it.
	 */
	struct CloneDummy final {
	private:
		constexpr CloneDummy() = default;
		friend constexpr CloneDummy makeCloneDummy();
	};

	constexpr CloneDummy makeCloneDummy() { return {}; }

	constexpr CloneDummy CLONE = makeCloneDummy();
}

/**
 * @brief Macro to have generic access to class type for other macros.
 */
#define THIS_CLASS(this_class) \
                               \
private:                       \
	using ThisClass = this_class

/**
 * @brief Macro to have generic access to parent class type for other macros.
 */
#define PARENT_CLASS(parent_class) \
                                   \
private:                           \
	using ParentClass = parent_class

/**
 * @brief Helper macro for ELEMENT_CLONE_DECL that adds a single argument to the initializer list.
 */
#define CONSTRUCTOR_VALUE_COPY(member) , member(other.member)

/**
 * @brief Clone constructor default declaration/definition macro.
 *
 * The arguments are all of the non-children data in the element that need to be copied.
 */
#define ELEMENT_CLONE_DECL(element, ...)                                 \
	explicit element(const pst::CloneDummy clone, const element& other): \
		  ParentClass(clone, other) FOR_EACH(CONSTRUCTOR_VALUE_COPY, __VA_ARGS__) {}

/**
 * @brief cloneElement signature macro. The function is used to manage cloning in the lowest level
 * (final) elements.
 */
#define CLONE_SIGNATURE() MBox<LangElement> cloneElement() const

/**
 * @brief Default implementation of cloneElement, has to be added to lowest level (final) elements.
 */
#define CLONE_SIGNATURE_DEFAULT_OVERRIDE()                                 \
	MBox<LangElement> cloneElement() const override {                      \
		static_assert(std::is_final_v<ThisClass>);                         \
		auto out = base::makeBox<ThisClass>(pst::makeCloneDummy(), *this); \
		out->cloneSubElements(*this);                                      \
		return out;                                                        \
	}


/**
 * @brief Declaration of cloneSubElements which is responsible for copying the children from the
 * element in the argument.
 */
#define CLONE_SUBELEMENTS() \
                            \
protected:                  \
	void cloneSubElements(const ThisClass&)


/**
 * @brief Macro that combines all of the macros that are used in lowest level (final) elements.
 */
#define SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(element, parent_class, ...) \
                                                                        \
public:                                                                 \
	ELEMENT_CLONE_DECL(element, __VA_ARGS__);                           \
                                                                        \
private:                                                                \
	THIS_CLASS(element);                                                \
	PARENT_CLASS(parent_class);                                         \
	CLONE_SIGNATURE_DEFAULT_OVERRIDE()

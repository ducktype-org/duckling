#pragma once
#include <base/for_each.hpp>
#include <base/type_traits.hpp>

/**
 * @brief Helper for MAKE_VISITOR. Creates virtual method implementation.
 */
#define VISITOR_METHOD(type) virtual void visit##type(const type& val) = 0;

/**
 * @brief Helper for MAKE_VISITOR. Creates empty implementation.
 */
#define VISITOR_EMPTY_METHOD(type) \
	void visit##type(const type&) override {}

/**
 * @brief Helper for MAKE_VISITOR. Creates panicky implementation.
 */
#define VISITOR_PANIC_METHOD(tp, type)                                    \
	void visit##type(const type&) override {                              \
		CORE_PANIC(base::typeName<tp>(), " visitor has visited: " #type); \
	}


/**
 * @brief Helper for MAKE_VISITOR. Creates visitor classes.
 */
#define MAKE_VISITOR_IMPL(BaseName, PanickyName, EmptyName, ...)     \
	class BaseName {                                                 \
	public:                                                          \
		virtual ~BaseName() = default;                               \
		FOR_EACH(VISITOR_METHOD, __VA_ARGS__)                        \
	};                                                               \
                                                                     \
	class EmptyName: public BaseName {                               \
	public:                                                          \
		~EmptyName() override = default;                             \
		FOR_EACH(VISITOR_EMPTY_METHOD, __VA_ARGS__)                  \
	};                                                               \
                                                                     \
	class PanickyName: public BaseName {                             \
	public:                                                          \
		~PanickyName() override = default;                           \
		FOR_EACH_ARG(VISITOR_PANIC_METHOD, PanickyName, __VA_ARGS__) \
	}

/**
 * @brief Creates visitors. Ex:
 * ```cpp
 * struct Base { virtual void acceptVisitor(MyVisitor& vis) = 0; };
 * struct A : Base { void acceptVisitor(MyVisitor& vis) { vis.visitA(*this); } };
 * struct B : Base { void acceptVisitor(MyVisitor& vis) { vis.visitB(*this); } };
 * struct C : Base { void acceptVisitor(MyVisitor& vis) { vis.visitC(*this); } };
 * MAKE_VISITOR(My, A, B, C);
 * ```
 * Creates MyVisitor, MyVisitorPanicky and MyVisitorEmpty
 * which are visitors for the A, B, C classes.
 * * MyVisitor has no implementations.
 * * MyVisitorEmpty has empty implementations.
 * * MyVisitorPanicky methods panic by default.
 *
 */
#define MAKE_VISITOR(name, ...) \
	MAKE_VISITOR_IMPL(name##Visitor, name##VisitorPanicky, name##VisitorEmpty, __VA_ARGS__)

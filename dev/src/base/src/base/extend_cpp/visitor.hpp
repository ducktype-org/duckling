#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/preproc/for_each.hpp>

/**
 * @brief Helper for MAKE_VISITOR. Default method interface.
 * @note You can customize visitor interface by using `MAKE_VISITOR_CUSTOM_INTERFACE`.
 */
#define VISITOR_DEFAULT_METHOD_INTERFACE(type) void visit##type(const type&)

/**
 * @brief Helper for MAKE_VISITOR. Creates virtual method implementation.
 */
#define VISITOR_METHOD(INTERFACE, type) virtual INTERFACE(type) = 0;

/**
 * @brief Helper for MAKE_VISITOR. Creates empty implementation.
 */
#define VISITOR_EMPTY_METHOD(INTERFACE, type) \
	INTERFACE(type) override {}

/**
 * @brief Helper for MAKE_VISITOR. Creates panicky implementation.
 */
#define VISITOR_PANIC_METHOD(INTERFACE, name, type) \
	INTERFACE(type) override { CORE_PANIC(base::typeName<name>(), " visitor has visited: " #type); }


/**
 * @brief Helper for MAKE_VISITOR. Creates visitor classes.
 */
#define MAKE_VISITOR_IMPL(BaseName, PanickyName, EmptyName, INTERFACE, ...)      \
	class BaseName {                                                             \
	public:                                                                      \
		virtual ~BaseName() = default;                                           \
		FOR_EACH_ARG(VISITOR_METHOD, INTERFACE, __VA_ARGS__)                     \
	};                                                                           \
                                                                                 \
	class EmptyName: public BaseName {                                           \
	public:                                                                      \
		~EmptyName() override = default;                                         \
		FOR_EACH_ARG(VISITOR_EMPTY_METHOD, INTERFACE, __VA_ARGS__)               \
	};                                                                           \
                                                                                 \
	class PanickyName: public BaseName {                                         \
	public:                                                                      \
		~PanickyName() override = default;                                       \
		FOR_EACH_2ARG(VISITOR_PANIC_METHOD, INTERFACE, PanickyName, __VA_ARGS__) \
	}

/**
 * @brief Creates visitors, but uses custom visitor method interface `interface`.
 */
#define MAKE_VISITOR_CUSTOM_INTERFACE(name, INTERFACE, ...)                             \
	MAKE_VISITOR_IMPL(                                                                  \
		name##Visitor, name##VisitorPanicky, name##VisitorEmpty, INTERFACE, __VA_ARGS__ \
	)

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
	MAKE_VISITOR_CUSTOM_INTERFACE(name, VISITOR_DEFAULT_METHOD_INTERFACE, __VA_ARGS__)

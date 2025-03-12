#pragma once
#include <base/for_each.hpp>

#define VISITOR_METHOD(type) virtual void visit##type(const type& val) = 0;

#define VISITOR_EMPTY_METHOD(type, ...) \
	void visit##type(const type&) override {}

#define VISITOR_PANIC_METHOD(type) \
	void visit##type(const type&) override { CORE_PANIC("Panicky visitor has visited: " #type); }

#define VISITOR_EMPTY_METHODS_AGAIN(...) __VA_OPT__(VISITOR_EMPTY_METHODS(__VA_ARGS__))

#define MAKE_VISITOR_IMPL(BaseName, PanickyName, EmptyName, ...) \
	class BaseName {                                             \
	public:                                                      \
		virtual ~BaseName() = default;                           \
		FOR_EACH(VISITOR_METHOD, __VA_ARGS__)                    \
	};                                                           \
                                                                 \
	class EmptyName: public BaseName {                           \
	public:                                                      \
		~EmptyName() override = default;                         \
		FOR_EACH(VISITOR_EMPTY_METHOD, __VA_ARGS__)              \
	};                                                           \
                                                                 \
	class PanickyName: public BaseName {                         \
	public:                                                      \
		~PanickyName() override = default;                       \
		FOR_EACH(VISITOR_PANIC_METHOD, __VA_ARGS__)              \
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

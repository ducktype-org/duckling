#include "src/printer/printer.hpp"

#include <iostream>

namespace N {
	i32 foo(i32 a, i32 b) {
		for (i32 i = 0; i < b; i++) a++;
		return a + b;

		switch (1 + 1) {
		case 2: {
			a++;
			break;
		}

		default:
			break;
		}
	}

	class MyType {
		i32 a;  // a

		MyType(i32 a): a(a) {}

		virtual void fillSymTable([[maybe_unused]] symtable::ScopeId      parent_scope,
		                          [[maybe_unused]] symtable::SymbolTable& symtable) {}
	};

	template<typename T>
	class MyTemplateType {
	private:
		i32         a;
		mutable i32 b;

	public:
		i32 aaa;

		void method(i32 a, i32 b, i32 c);
	};

	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6,
	         typename T7>
	class MyLongTemplateType {};
};

/**
 * @brief
 * aaaaa
 */
namespace N1 {
	namespace N2 {
		i32 aLotOfParams([[maybe_unused]] const volatile u64 a,
		                 [[maybe_unused]] const volatile u64 b,
		                 [[maybe_unused]] const volatile u64 c, i32 y, i32 p, i32 aaaaaaa,
		                 char yyyyyy) {
			i32 var;
			while (true)
				while (true)
					while (true)
						while (true)
							while (true)
								while (true)
									while (true)
										while (true)
											while (true)
												while (true) {}

			i32 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			i32 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			i32 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			i32 ccccccccccccccccccccccccccccccccccccccccc;

			bool value = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
			                   + aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
			              == aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
			          && (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
			              > ccccccccccccccccccccccccccccccccccccccccc);
		}
	}
}

int main() {
	i32 a;

	printer::Console console;
	console.add({
		{
         {
				{ "Hello, World! This is the first message, " },
				{ "So it will be the only one you see before error, when GeneralMax is set to 1." },
			}, printer::MessageType::HINT,
         0, printer::Color::RESET,
         printer::Color::RESET,
		 },
		{
         {
				{ "R", printer::Color::BRIGHT_RED },
				{ "A", printer::Color::YELLOW },
				{ "I", printer::Color::BRIGHT_YELLOW },
				{ "N", printer::Color::BRIGHT_GREEN },
				{ "B", printer::Color::BRIGHT_BLUE },
				{ "O", printer::Color::BRIGHT_CYAN },
				{ "W", printer::Color::MAGENTA },
			}, printer::MessageType::NOTE,
         1, },
		{
         {
				{ "This message uses a default color. " },
				{ "I can still manually change it. ", printer::Color::GREEN },
				{ "But all messages that do not specify color display it. This is the second hint message,"
	              " it won't be displayed when hints are limited to 1." },
			}, printer::MessageType::HINT,
         2, printer::Color::BLUE,
		 },
		{
         {
				{ "It's also possible now to change the " },
				{ "BACKGROUND", printer::Color::DEFAULT, printer::Color::YELLOW },
				{ ". " },
				{ "How cool is that ?", printer::Color::RED, printer::Color::CYAN },
			}, printer::MessageType::ERROR,
         3, },
		{
         {
				{ "How about default message backgrounds? Also this is the only message of level 4 or above" },
				{ ", so it's the only one that appears when printing with MinLevel 4 on ALL." },
			}, printer::MessageType::DEBUG,
         4, printer::Color::RESET,
         printer::Color::GRAY,
		 },
	});


	console.add({
		{
         {
				{ "Hello, World! This is the first message, " },
				{ "So it will be the only one you see before error, when GeneralMax is set to 1." },
			}, printer::MessageType::HINT,
         0, printer::Color::RESET,
         printer::Color::RESET,
		 },
	});


	const i32 ah     = 0;
	const i32 ssssss = 0;

	int& a;
	int& a;
	i32  a[5][5];

	for (auto v : values) {}

	foo();
}

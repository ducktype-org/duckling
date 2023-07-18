#include "src/printer/printer.hpp"
#include <iostream>

namespace N {
	int foo(int a, int b) {
		for (int i = 0; i < b; i++) {
			a++;
		}
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
		int a; // a
		MyType(int a): a(a) {}

		virtual void fillSymTable([[maybe_unused]] symtable::ScopeId parent_scope,
		                          [[maybe_unused]] symtable::SymbolTable& symtable) {}
	};

	template<typename T>
	class MyTemplateType {
	private:
		int a;
		mutable int b;

	public:
		int aaa;

		void method(int a, int b, int c);
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
		int aLotOfParams([[maybe_unused]] const volatile unsigned long long int a,
		                 [[maybe_unused]] const volatile unsigned long long int b,
		                 [[maybe_unused]] const volatile unsigned long long int c, int y, int p,
		                 int aaaaaaa, char yyyyyy) {
			int var;
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

			int aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			int aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			int aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;
			int ccccccccccccccccccccccccccccccccccccccccc;

			bool value = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa +
			                     aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ==
			                 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa &&
			             (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa >
			              ccccccccccccccccccccccccccccccccccccccccc);
		}
	}
}

int main() {
	int a;

	printer::Console console;
	console.add({
		{
			{
				{"Hello, World! This is the first message, "},
				{"So it will be the only one you see before error, when GeneralMax is set to 1."},
			},
			printer::MessageType::HINT,
			0,
			printer::Color::RESET,
			printer::Color::RESET,
		},
		{
			{
				{"R", printer::Color::BRIGHT_RED},
				{"A", printer::Color::YELLOW},
				{"I", printer::Color::BRIGHT_YELLOW},
				{"N", printer::Color::BRIGHT_GREEN},
				{"B", printer::Color::BRIGHT_BLUE},
				{"O", printer::Color::BRIGHT_CYAN},
				{"W", printer::Color::MAGENTA},
			},
			printer::MessageType::NOTE,
			1,
		},
		{
			{
				{"This message uses a default color. "},
				{"I can still manually change it. ", printer::Color::GREEN},
				{"But all messages that do not specify color display it. This is the second hint message,"
	             " it won't be displayed when hints are limited to 1."},
			},
			printer::MessageType::HINT,
			2,
			printer::Color::BLUE,
		},
		{
			{
				{"It's also possible now to change the "},
				{"BACKGROUND", printer::Color::DEFAULT, printer::Color::YELLOW},
				{". "},
				{"How cool is that ?", printer::Color::RED, printer::Color::CYAN},
			},
			printer::MessageType::ERROR,
			3,
		},
		{
			{
				{"How about default message backgrounds? Also this is the only message of level 4 or above"},
				{", so it's the only one that appears when printing with MinLevel 4 on ALL."},
			},
			printer::MessageType::DEBUG,
			4,
			printer::Color::RESET,
			printer::Color::GRAY,
		},
	});


	console.add({
		{
			{
				{"Hello, World! This is the first message, "},
				{"So it will be the only one you see before error, when GeneralMax is set to 1."},
			},
			printer::MessageType::HINT,
			0,
			printer::Color::RESET,
			printer::Color::RESET,
		},
	});


	const int ah = 0;
	const int ssssss = 0;

	int& a;
	int& a;
	int a[5][5];

	for (auto v: values) {}

	foo();
}

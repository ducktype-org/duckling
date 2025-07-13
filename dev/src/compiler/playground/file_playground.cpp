#include <filesystem/file.hpp>

#include <base/exceptions.hpp>

#include <iostream>

using namespace fs;

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./file_testing file_name\n";
		return 1;
	}
	fs::File file(argv[1]);

	auto out = file.getContent();
	std::cout << out.view().size() << "\n";
	for (usize i = 0; i < out.view().size(); i++) std::cout << (uint) out.view()[i] << "\n";
}

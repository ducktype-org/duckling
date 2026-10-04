
#include <filesystem/file.hpp>
#include <init/init.hpp>

#include <iostream>

using namespace fs;

int main(int argc, char** argv) {
	init::InitObject _;

	if (argc != 2) {
		std::cerr << "usage: ./file_testing file_name\n";
		return 1;
	}
	fs::File file(argv[1]);

	auto out = file.getContent();
	std::cout << out.view().size() << "\n";
	for (usize i = 0; i < out.view().size(); i++)
		std::cout << static_cast<unsigned>(out.view()[i]) << "\n";
}

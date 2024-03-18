#include "frontend/fs_parser/fs_parser.hpp"
#include <iostream>

int counter = 0;

void print(const compiler::frontend::FsTree& fsp) {
	for (const auto& f: fsp.iterFiles()) {
		counter++;
		std::cout << fsp.getRoot().absolutePath() << " ? " << f.first << '\n';
	}
	for (const auto& f: fsp.iterDirs()) {
		counter++;
		std::cout << fsp.getRoot().absolutePath() << " ? " << f.first << '\n';
	}
	for (const auto& f: fsp.iterDirs()) print(*f.second);
}

int main() {
	//	auto fsp = compiler::frontend::FsTree::create("/home/mateusz");
	auto fsp = compiler::frontend::FsTree::create("../../playground/.");
	print(*fsp);
	std::cout << "All files and dirs: " << counter << '\n';
}

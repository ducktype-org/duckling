#include <iostream>
#include <filesystem/file.hpp>

int main() {
	const auto f = fs::FilePath::createTempFile("Content");
	std::cout << f.getContent().view().stringView() << '\n';
	std::cout << f.absolutePath() << '\n';
}

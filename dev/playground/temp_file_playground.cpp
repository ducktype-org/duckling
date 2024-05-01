#include <iostream>
#include <filesystem/file.hpp>

int main() {
	const auto f = fs::FilePath::createTempFile("Content");
	std::cout << f.getContent().view().stringView() << '\n';
	std::cout << f.absolutePath() << '\n';

	const auto temp_dir = fs::FilePath::createTempDirectory();
	std::cout << temp_dir.absolutePath() << '\n';
	const auto f1 = temp_dir.createTempFileIn("f1");
	const auto f2 = temp_dir.createTempFileIn("f2", "customName");
	std::cout << f1.getContent().view().stringView() << '\n';
	std::cout << f1.absolutePath() << '\n';
	std::cout << f2.getContent().view().stringView() << '\n';
	std::cout << f2.absolutePath() << '\n';
}

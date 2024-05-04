#include <iostream>
#include <filesystem/file.hpp>

int main() {
	// Create a temporary file with content: "Content"
	const auto f = fs::FilePath::createTempFile("Content");
	std::cout << "f content: " << f.getContent().view().stringView() << '\n';
	std::cout << "f path: " << f.absolutePath() << '\n';

	// Create a temporary directory.
	const auto temp_dir = fs::FilePath::createTempDirectory();
	std::cout << "temp_dir path: " << temp_dir.absolutePath() << '\n';

	// Create a new file in `temp_dir` with content: "f1".
	const auto f1 = temp_dir.createTempFileIn("f1");
	std::cout << "f1 content: " << f1.getContent().view().stringView() << '\n';
	std::cout << "f1 path: " << f1.absolutePath() << '\n';

	// Create a new file in `temp_dir` named "customName" with content: "f2".
	const auto f2 = temp_dir.createTempFileIn("f2", "customName");
	std::cout << "f2 content: " << f2.getContent().view().stringView() << '\n';
	std::cout << "f2 path: " << f2.absolutePath() << '\n';
}

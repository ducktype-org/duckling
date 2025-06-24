#include <filesystem/file.hpp>

#include <iostream>

int main() {
	// Create a temporary file with content: "Content"
	const auto f = fs::FileManager::createRandomTempFile("Content");
	std::cout << "f content: " << f.getContent().view().stringView() << '\n';
	std::cout << "f path: " << f.absolutePath() << '\n';

	// Create a temporary directory.
	const auto temp_dir = fs::FileManager::createTempDirectory();
	std::cout << "temp_dir path: " << temp_dir.absolutePath() << '\n';

	// Create a new file in `temp_dir` with content: "f1".
	const auto f1 = fs::FileManager::createFileIn(temp_dir, "f1");
	std::cout << "f1 content: " << f1.getContent().view().stringView() << '\n';
	std::cout << "f1 path: " << f1.absolutePath() << '\n';

	// Create a new file in `temp_dir` named "customName" with content: "f2".
	const auto f2 = fs::FileManager::createFileIn(temp_dir, "f2", "customName");
	std::cout << "f2 content: " << f2.getContent().view().stringView() << '\n';
	std::cout << "f2 path: " << f2.absolutePath() << '\n';
}

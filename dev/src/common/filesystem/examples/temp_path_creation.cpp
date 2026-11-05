// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <filesystem/file.hpp>

#include <iostream>

int main() {
	// Create a temporary file with content: "Content"
	const auto f = base::valueOrPanic(fs::FileManager::createRandomTempFile("Content"));
	std::cout << "f content: " << base::valueOrPanic(f.getContent()).view().stringView() << '\n';
	std::cout << "f path: " << f.getFilePath().native() << '\n';

	// Create a temporary directory.
	const auto temp_dir = base::valueOrPanic(fs::FileManager::createRandomTempDirectory());
	std::cout << "temp_dir path: " << temp_dir.getFilePath().native() << '\n';

	// Create a new file in `temp_dir` with content: "f1".
	const auto f1 = base::valueOrPanic(temp_dir.createSubFile("f1"));
	std::cout << "f1 content: " << base::valueOrPanic(f1.getContent()).view().stringView() << '\n';
	std::cout << "f1 path: " << f1.getFilePath().native() << '\n';

	// Create a new file in `temp_dir` named "customName" with content: "f2".
	const auto f2 = base::valueOrPanic(temp_dir.createSubFile("f2", "customName"));
	std::cout << "f2 content: " << base::valueOrPanic(f2.getContent()).view().stringView() << '\n';
	std::cout << "f2 path: " << f2.getFilePath().native() << '\n';
}

/**
 * @file lexerContext.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "lexer.hpp"

#include <diagnostic/logger.hpp>
#include <token_file/file.hpp>
#include <fstream>
#include <base/exceptions.hpp>

namespace lexer {
	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		std::cerr << "DEBUG [tokenizeFile]: Attempting to read file for tokenization: "
				  << path.strView() << '\n';
		std::ifstream debug_file_stream(
			path.strView().data(), std::ios::binary | std::ios::ate
		);  // Otwórz w trybie binarnym, wskaźnik na koniec

		if (!debug_file_stream.is_open()) {
			std::cerr << "DEBUG [tokenizeFile]: Error: Could not open file '" << path.strView()
					  << "' for debug reading." << '\n';
		} else {
			std::streamsize size = debug_file_stream.tellg();
			debug_file_stream.seekg(0, std::ios::beg);  // Wróć na początek

			std::string buffer((u64)size, '\0');             // Utwórz string o odpowiednim rozmiarze
			if (debug_file_stream.read(&buffer[0], size)) {
				std::cerr << "DEBUG [tokenizeFile]: Content of file '" << path.strView()
						  << "' (size: " << size << " bytes):\n---BEGIN FILE CONTENT---\n"
						  << buffer << "\n---END FILE CONTENT---" << '\n';

				// Opcjonalnie: wypisz pierwsze N bajtów jako hex, aby sprawdzić BOM lub inne
				// niewidoczne znaki
				std::cerr << "DEBUG [tokenizeFile]: Hex dump of first 32 bytes:\n";
				for (std::streamsize i = 0; i < std::min(size, (std::streamsize) 32); ++i) {
					std::cerr << std::hex << std::setw(2) << std::setfill('0')
							  << static_cast<int>(static_cast<unsigned char>(buffer[(u64)i])) << " ";
					if ((i + 1) % 16 == 0) std::cerr << '\n';
				}
				std::cerr << std::dec << '\n';  // Reset to decimal output

			} else {
				std::cerr << "DEBUG [tokenizeFile]: Error: Could not read content from file '"
						  << path.strView() << "' for debug." << '\n';
			}
			debug_file_stream.close();  // Zamknij strumień debugowania
		}
		auto file = tokenizer::makeTokenFile(path);
		file->tokenize();
		if (file->getLogger().bad()) {
			file->getLogger().dumpLog(false, std::cerr);
			throw base::LogicError("syntax error during lexing");
		}
		return file;
	}

}

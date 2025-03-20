

#include <vm/preprocessor/parser/elements.hpp>
#include <token_file/file.hpp>
#include <deque>
#include "errors.hpp"

namespace vm::parser {

	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		lang_def::setKeywordMode(lang_def::KeywordMode::DuckBC);
		return lexer::tokenizeFile(path);
	}

	MBox<ParsedFile> parseFile(Ref<tokenizer::TokenFile> file, dia::Logger& log) {
		const lexer::TokenData& td = file->getTokenData();

		F8ParserState state(
			tpc::TokenStream(td.tokens, td.bof_sentinel, td.eof_sentinel, 0, td.tokens.size()), log
		);

		return ParsedFile::parse(state);
	}

	std::expected<std::vector<ParsedFile>, std::string> parse(const std::vector<fs::FilePath>& files
	) {
		// So that they dont't die?
		static std::vector<Box<tokenizer::TokenFile>> tokenized_files;
		auto                                          log = dia::Logger();
		std::vector<ParsedFile>                       parsed_files;

		for (const auto& file: files) {
			tokenized_files.emplace_back(tokenizeFile(file));
			auto maybe_parsed = parseFile(tokenized_files.back().refMut(), log);

			if (log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			} else {
				parsed_files.push_back(std::move(*maybe_parsed));
			}


			// 	for (auto& func: parsed->functions) {
			// 		auto func_name = func->name.value;
			// 		if (parsed_program.name_to_func.contains(func_name)) {
			// 			auto msg
			// 				=
			// makeBox<vm::parser::DuplicateFunctionDeclarationError>(*func->position);
			// auto dup_func = parsed_program.name_to_func.atMaybe(func_name);
			// 			msg->addNote(makeBox<vm::parser::DuplicatedFunctionDeclarationNote>(
			// 				*dup_func.value()->position
			// 			));
			// 			log.log(std::move(msg));
			// 		}

			// 		parsed_program.functions.push_back(std::move(func));
			// 		parsed_program.name_to_func.put(
			// 			func_name, parsed_program.functions.back().refMut()
			// 		);
			// 	}
		}

		return parsed_files;
	}
}

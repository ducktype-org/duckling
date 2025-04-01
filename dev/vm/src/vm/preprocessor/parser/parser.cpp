
#include "errors.hpp"

#include <token_file/file.hpp>

#include <vm/preprocessor/parser/elements.hpp>

#include <deque>
#include <expected>

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

	std::expected<std::vector<ParsedFile>, dia::Logger> parse(const std::vector<fs::FilePath>& files
	) {
		// So that they dont't die?
		static std::vector<Box<tokenizer::TokenFile>> tokenized_files;
		auto                                          log = dia::Logger();
		std::vector<ParsedFile>                       parsed_files;

		for (const auto& file: files) {
			tokenized_files.emplace_back(tokenizeFile(file));
			auto maybe_parsed = parseFile(tokenized_files.back().refMut(), log);

			if (log.bad())
				return std::unexpected(std::move(log));
			else
				parsed_files.push_back(std::move(*maybe_parsed));
		}

		return parsed_files;
	}
}

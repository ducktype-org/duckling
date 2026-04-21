#include <diagnostic_interactive/logger.hpp>

#include <token_source/source.hpp>

#include <vm/loader/parser/elements.hpp>

#include <expected>

namespace vm::loader::parser {

	Box<tokenizer::TokenSource> tokenizeFile(const fs::File& path) {
		lang_def::setKeywordMode(lang_def::KeywordMode::DuckBC);
		auto source = tokenizer::makeTokenSource(path);
		source->tokenize();
		return source;
	}

	MBox<ParsedFile> parseFile(Ref<tokenizer::TokenSource> file, Ref<dia_int::Logger> int_log) {
		const lexer::TokenData& td = file->getTokenData();

		F8ParserState state(
			tpc::TokenStream(td.tokens, td.bof_sentinel, td.eof_sentinel, 0, td.tokens.size()),
			int_log
		);
		return ParsedFile::parse(state);
	}

	std::expected<std::vector<ParsedFile>, dia_int::Logger> parse(const std::vector<fs::File>& files
	) {
		// @TODO: Decide on a better position
		// So that they dont't die
		static std::vector<Box<tokenizer::TokenSource>> tokenized_files;
		auto                                            int_log = dia_int::Logger();
		std::vector<ParsedFile>                         parsed_files;

		for (const auto& file: files) {
			tokenized_files.emplace_back(tokenizeFile(file));
			auto maybe_parsed = parseFile(tokenized_files.back().refMut(), &int_log);
			if (int_log.bad()) return std::unexpected(std::move(int_log));
			parsed_files.push_back(std::move(*maybe_parsed));
		}

		return parsed_files;
	}
}


#include <token_file/file.hpp>

#include <vm/loader/parser/elements.hpp>

#include <expected>

namespace vm::loader::parser {

	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		lang_def::setKeywordMode(lang_def::KeywordMode::DuckBC);
		return lexer::tokenizeFile(path);
	}

	MBox<ParsedFile> parseFile(Ref<tokenizer::TokenFile> file, dia::Logger& log) {
		std::cerr << "[DEBUG parseFile] === ENTERING for file: " << file->getPath().strView()
				  << " ===\n";
		std::cerr << "[DEBUG parseFile] Initial logger state: good()=" << log.good()
				  << ", errorCount=" << log.messageCount() << '\n';
		const lexer::TokenData& td = file->getTokenData();
		std::cerr << "[DEBUG parseFile] Got TokenData, before state. Logger state: good()=" << log.good()
				  << ", errorCount=" << log.messageCount() << '\n';
		F8ParserState state(
			tpc::TokenStream(td.tokens, td.bof_sentinel, td.eof_sentinel, 0, td.tokens.size()), log
		);
		std::cerr << "[DEBUG parseFile] After creating F8ParserState. Logger state: good()="
				  << log.good() << ", errorCount=" << log.messageCount() << '\n';

		auto res = ParsedFile::parse(state);

		std::cerr << "[DEBUG parseFile] After parse. Logger state: good()="
				  << log.good() << ", errorCount=" << log.messageCount() << '\n';

		return res;
	}

	std::expected<std::vector<ParsedFile>, dia::Logger> parse(const std::vector<fs::FilePath>& files
	) {
		// @TODO: Decide on a better position
		// So that they dont't die
		static std::vector<Box<tokenizer::TokenFile>> tokenized_files;
		auto                                          log = dia::Logger();
		assert(log.good() && "Nowy logger powinien być 'good' na starcie!");
		std::cerr << "[DEBUG] Nowy logger w parse(): good()=" << log.good()
				  << ", errorCount=" << log.messageCount(dia::Message::Severity::Error) << '\n';
		std::vector<ParsedFile> parsed_files;

		for (const auto& file: files) {
			std::cerr << "[DEBUG] Przed tokenizeFile dla: "
					  << file.strView()  // Załóżmy, że FilePath ma toString()
					  << ", log.good()=" << log.good() << '\n';

			tokenized_files.emplace_back(tokenizeFile(file));
			std::cerr << "[DEBUG] Przed parseFile, log.good()=" << log.good()
					  << ", przetwarzany plik: ostatni z tokenized_files" << '\n';

			auto maybe_parsed = parseFile(tokenized_files.back().refMut(), log);
			if (log.bad()) {
				std::cerr << "[DEBUG] log.bad() jest true, zwracam błąd." << '\n';
				log.dumpLog(true, std::cerr);  // UWAGA: To może być dużo danych
				return std::unexpected(std::move(log));
			}
			parsed_files.push_back(std::move(*maybe_parsed));
		}

		return parsed_files;
	}
}

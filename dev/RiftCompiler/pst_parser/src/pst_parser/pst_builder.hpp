#include <token_file/file.hpp>
#include "rift_parser_base.hpp"
#include "parser.hpp"

namespace pst {
	template<typename T>
	concept ParseAble = requires(pst::RiftParserState& state) {
		requires std::derived_from<T, RiftElement>;
		{ T::parse(state) } -> std::same_as<ParserRef<T>>;
	};

	template<std::derived_from<RiftElement> Element>
	class PSTBuilder {
	private:
		tokenizer::OwnFile      file;
		ParserRef<Element>      element;
		std::vector<ImportType> imports;

		/**
		 * @brief Requires that the file was successfully tokenized.
		 */
		void parse()
		requires ParseAble<Element>
		{
			lexer::TokenData& token_data = file->getTokenData();
			RiftParserState   state(
                tpc::TokenStream(
                    token_data.tokens,
                    tpc::Token(token_data.eof_sentinel),
                    0,
                    token_data.tokens.size()
                ),
                file->getLogger()
            );
			element = Element::parse(state);
			imports = std::move(state).extractState();
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		explicit PSTBuilder(std::string_view content)
		requires ParseAble<Element>
			  : file(tokenizer::makeTokenFile(fs::FilePath::createTempFile(content))) {
			pst::init();
			if (!file->tokenize()) return;
			parse();
		}

	public:
		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		PSTBuilder(tokenizer::OwnFile&& file)
		requires ParseAble<Element>
			  : file(std::move(file)) {
			pst::init();
			if (getLogger().bad()) return;
			parse();
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PSTBuilder(const fs::FilePath& path)
		requires ParseAble<Element>
			  : file(tokenizer::makeTokenFile(path)) {
			pst::init();
			if (!file->tokenize()) return;
			parse();
		}

		static PSTBuilder fromContents(std::string_view contents)
		requires ParseAble<Element>
		{
			return PSTBuilder(contents);
		}

		[[nodiscard]]
		const std::vector<ParserCBorrowRef<Import>>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		const dia::Logger& getLogger() const {
			return file->getLogger();
		}

		[[nodiscard]]
		tokenizer::BorrowFile getFile() const {
			return file.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<Element> getTopLevelElement() const {
			return element.borrow();
		}

		PSTBuilder(PSTBuilder&& other) noexcept:
			  file(std::move(other.file)),
			  element(std::move(other.element)),
			  imports(std::move(other.imports)) {}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}

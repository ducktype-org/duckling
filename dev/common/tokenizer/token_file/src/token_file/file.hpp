
#include "base/borrow_pointer.hpp"
#include <base/unique_pointer.hpp>
#include <filesystem/file.hpp>
#include <lexer/token.hpp>

#include <set>

namespace tokenizer {
	/**
	 * @brief Class managing source file data access and token metadata
	 */
	class TokenFile {
	private:
		fs::FilePath path;	
		base::Optional<fs::FileContent> content;
		base::Optional<lexer::CharArray> decoded;
		base::Optional<lexer::TokenData> token_data;

		/**
		 * @brief Stores line bounds in file.
		 */
		std::vector<std::pair<usize, usize>> lines;
		/**
		 * @brief Stores pairs (source file index, line number)
		 */
		std::set<std::pair<usize, usize>> line_begins;

		/**
		 * @brief Controls manual file locking
		 * 
		 * When true the file content is kept in memory.
		 * When false the file content is loaded to memory when needed.
		 * 
		 * @note For now we leave the content available the whole time.
		 * @note For now false is not completely implemented.
		 */
		bool lock = true;

		base::borrow_ptr<TokenFile> self;

		TokenFile(fs::FilePath&&);

		template <class... Ts>
		friend base::unique_ptr<TokenFile> makeTokenFile(Ts&&... args);
	public:
		TokenFile(const TokenFile&) = delete;
		TokenFile() = delete;

		TokenFile(TokenFile&&) = default;

		[[nodiscard]] 
		std::pair<usize, usize> getLineColumn(usize source_pos);

		fs::FileContent getContent();		

		lexer::TokenData& getTokenData();
	};

	template <class... Ts>
	base::unique_ptr<TokenFile> makeTokenFile(Ts&&... args) {
		auto ptr = base::make_unique<TokenFile>(std::forward(args...));
		ptr->self = ptr.borrow_mut();
		return ptr;
	}
}
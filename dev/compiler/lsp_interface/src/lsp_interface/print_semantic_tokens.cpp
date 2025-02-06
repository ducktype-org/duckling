#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <base/variant.hpp>
#include <base/stringifyable_enum.hpp>
#include <lexer/token.hpp>

#include <string>
#include <vector>
#include <map>

std::string jsonList(const std::vector<std::string>& list) {
	std::string result = "[";
	for (const std::string& str: list) result += "\"" + str + "\",";
	if (result[result.length() - 1] == ',') result.pop_back();
	result += "]";

	return result;
}

std::string jsonDict(const std::map<std::string, std::string>& dict) {
	std::string result = "{";
	for (const auto& pair: dict) {
		const auto& key   = pair.first;
		const auto& value = pair.second;
		result += "\"";
		result += key;
		result += "\":";
		result += value;
		result += ",";
	}
	if (result[result.length() - 1] == ',') result.pop_back();
	result += "}";

	return result;
}

#define lTT lexer::Token::Type

std::string tokenTypeToStr(lTT type) {
	switch (type) {
		case lTT::Keyword: return "keyword";
		// case lTT::Identifier: return "";
		case lTT::NumLiteral: return "number";
		case lTT::String: return "string";
		case lTT::FormattedString: return "string";
		// case lTT::BracketGroup: return "";
		case lTT::Operator: return "operator";
		case lTT::Comment: return "comment";
		// case lTT::Special: return "";
		// case lTT::Empty: return "";
		// case lTT::Sentinel: return "";
		// case lTT::Error: return "";
		default: return "";
	}
}

std::string getSemanticTokens(MCRef<pst::LangElement> element) {
	std::vector<std::string> tokens;

	for (auto sub: element->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::LangElement::SubToken, token) {
				// add semantic token
				std::cout << (std::string){ token->getStrValue().data(), token->getStrValue().size() } << std::endl; 
				std::map<std::string, std::string> tokenData;

				auto position = token->getPosition();

				tokenData["line"] = std::to_string(position.getStartLineColumn().first);
				tokenData["startCharacter"] = std::to_string(position.getStartLineColumn().second);
				tokenData["length"] = std::to_string(position.getEnd() - position.getStart() + 1);
				tokenData["tokenType"] = tokenTypeToStr(token->getType());
				tokenData["tokenModifiers"] = "0"; // @TODO
				
				std::cout << jsonDict(tokenData) << std::endl;

				tokens.push_back(jsonDict(tokenData));
			}

			variant_case(pst::LangElement::ConstChild, child) {
				// recursive token generation
				getSemanticTokens(child);
			}
		}
	}

	return jsonList(tokens);
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./semantic_tokens_playground duckling_file\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);
	pst::PST<>   pst(file);

	if (pst.getLogger().bad()) {
		pst.getLogger().dumpLog(false, std::cerr);
		std::cerr << "\nThere are errors.\n";
		pst.dprint(std::cerr);
		std::cerr << "\n";
	}
	if (pst.getRootElement() != nullptr) {
		auto res = getSemanticTokens(pst.getRootElement());
		// std::cout << res << std::endl;
	}
}
#include "tokenizer.hpp"
#include <cctype>
#include <unordered_map>

namespace {
	std::unordered_map<std::string, TokenType> keywords = {
		{ "void", TokenType::TYPE_VOID },
		{ "bool", TokenType::TYPE_BOOL },
		{ "char", TokenType::TYPE_CHAR },
		{ "short", TokenType::TYPE_SHORT },
		{ "int", TokenType::TYPE_INT },
		{ "long", TokenType::TYPE_LONG },
		{ "float", TokenType::TYPE_FLOAT },
		{ "double", TokenType::TYPE_DOUBLE },
		{ "enum", TokenType::TYPE_ENUM },
		{ "struct", TokenType::TYPE_STRUCT },
		{ "const", TokenType::CONST },
		{ "unsigned", TokenType::UNSIGNED },
		{ "return", TokenType::RETURN },

		{ "if", TokenType::IF },
		{ "else", TokenType::ELSE},

		{ "do", TokenType::DO},
		{ "while", TokenType::WHILE},
		{ "for", TokenType::FOR},
		{ "break", TokenType::BREAK},
		{ "continue", TokenType::CONTINUE},

		{ ";", TokenType::SEMICOLON },
		{ "(", TokenType::OPEN_PARENTH },
		{ ")", TokenType::CLOSED_PARENTH },
		{ "{", TokenType::OPEN_BRACE },
		{ "}", TokenType::CLOSED_BRACE },
		{ "[", TokenType::OPEN_BRACKET},
		{ "]", TokenType::CLOSED_BRACKET },

		{ "=" , TokenType::ASSIGN },
		{ "!" , TokenType::NOT },
		{ "||" , TokenType::OR },
		{ "&&" , TokenType::AND },
		{ "^" , TokenType::XOR },
		{ ">" , TokenType::GREATER },
		{ "<" , TokenType::LESS },

		{ "+" , TokenType::PLUS },
		{ "-" , TokenType::MINUS },
		{ "*" , TokenType::STAR },
		{ "/" , TokenType::SLASH },
		{ "%" , TokenType::PERCENT },

		{ "," , TokenType::COMMA },
		{ "." , TokenType::DOT },

		{ "+=" , TokenType::PLUS_ASSIGN },
		{ "-=" , TokenType::MINUS_ASSIGN },
		{ "*=" , TokenType::STAR_ASSIGN },
		{ "/=" , TokenType::SLASH_ASSIGN },

		{ "==", TokenType::EQUALS },
		{ "!=" , TokenType::NOT_ASSIGN },
		{ "|=" , TokenType::OR_ASSIGN },
		{ "&=" , TokenType::AND_ASSIGN },
		{ "^=" , TokenType::XOR_ASSIGN },

		{ "++" , TokenType::INCREMENT },
		{ "--", TokenType::DECREMENT },
	};

	template <typename T>
	bool IsInBounds(const T& value, const T& low, const T& high) {
		return !(value < low) && !(high < value);
	}
}

Tokenizer::Tokenizer(std::string_view view) {
	std::string word;
	std::size_t idx = 0;
	std::uint64_t lineCtr = 1, charCtr = 0;

	while (idx < view.size()) {
		word.clear();
		charCtr++;
		// Skip whitespace
		while (idx < view.size() && std::isspace(view[idx])) { 
			idx++;
			charCtr++;
		}
		if (idx >= view.size()) {
			break;
		}

		if (std::isalnum(view[idx])) {
			if (std::isalpha(view[idx])) {
				while (idx < view.size() && (std::isalnum(view[idx]) || view[idx] == '_')) {
					word += view[idx++];
				};

				TokenType type = TokenType::IDENT;
				if (keywords.contains(word)) { type = keywords[word]; }
				toks.emplace_back(type, lineCtr, charCtr, word);
				charCtr += word.size();
			}
			if (std::isdigit(view[idx])) {
				bool hasDot = false;
				while (idx < view.size() && (std::isdigit(view[idx]) || view[idx] == '.')) {
					if (view[idx] == '.') {
						if (hasDot) throw std::string("Already present dot");
						hasDot = true;
					}
					word += view[idx++];
				};

				toks.emplace_back(hasDot ? TokenType::FLOAT : TokenType::INTEGER, lineCtr, charCtr, word);
				charCtr += word.size();
			}
			continue;
		}

		word += view[idx];
		idx++;
		if (idx < view.size()) { 
			word += view[idx];
		}

		if (keywords.contains(word)) {
			if (word.size() > 1) {
				toks.emplace_back(keywords[word], lineCtr, charCtr++, word);
				idx++;
			}
			else {
				toks.emplace_back(keywords[word], lineCtr, charCtr, word);
			}
			continue;
		}
		word.pop_back();
		if (keywords.contains(word)) {
			toks.emplace_back(keywords[word], lineCtr, charCtr, word);
		}
	}

	toks.emplace_back();
}

const Token& Tokenizer::Get() const {
	return toks[currIdx];
}
const Token& Tokenizer::Next() {
	if (currIdx == toks.size() - 1) return toks.back();
	return toks[currIdx++];
}

void Tokenizer::Back() {
	if (currIdx != 0) currIdx--;
}
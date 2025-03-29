#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <string_view>

enum class TokenType: std::uint8_t {
	NONE,

	IDENT,

	FLOAT,
	INTEGER,

	TYPES_BEGIN,
	AUTO = TYPES_BEGIN,
	TYPE_VOID,
	TYPE_BOOL,
	TYPE_CHAR,
	TYPE_SHORT,
	TYPE_INT,
	TYPE_LONG,
	TYPE_FLOAT,
	TYPE_DOUBLE,
	TYPE_STRUCT,
	TYPE_ENUM,
	TYPES_END = TYPE_ENUM,

	SEMICOLON,
	OPEN_PARENTH,
	CLOSED_PARENTH,
	OPEN_BRACE,
	CLOSED_BRACE,
	OPEN_BRACKET,
	CLOSED_BRACKET,

	IF,
	ELSE,
	DO,
	WHILE,
	FOR,
	BREAK,
	CONTINUE,

	CONST,
	UNSIGNED,

	ASSIGN,
	NOT,
	OR,
	AND,
	XOR,
	LESS,
	GREATER,

	PLUS,
	MINUS,
	STAR,
	SLASH,
	PERCENT,

	COMMA,
	DOT,

	EQUALS,
	NOT_ASSIGN,
	OR_ASSIGN,
	AND_ASSIGN,
	XOR_ASSIGN,

	PLUS_ASSIGN,
	MINUS_ASSIGN,
	STAR_ASSIGN,
	SLASH_ASSIGN,

	INCREMENT,
	DECREMENT,

	RETURN
};
struct Token {
	TokenType type = TokenType::NONE;
	std::uint64_t line = 0, charOffset = 0;
	std::string value;

	bool IsOfType(TokenType type) const {
		return this->type == type;
	}
	template<typename ...Types>
	bool IsOfAnyType(std::same_as<TokenType> auto &&...types) const {
		return ((type == types)||...);
	}
};

class Tokenizer {
	std::vector<Token> toks;
	std::size_t currIdx = 0;

public:
	Tokenizer(std::string_view view);

	const Token& Get() const;
	const Token& Next();

	std::size_t GetIdx() const { return currIdx; }
	void SetIdx(std::size_t idx) { currIdx = idx; }

	void Back();
};
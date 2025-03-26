#pragma once

#include <cstdint>
#include <ostream>
#include <istream>
#include <stack>
#include "parser.hpp"

enum class InstructionCode : std::uint8_t {
	NONE = 255,
	NOP = 0,
	SKIP,		// skip N bytes
	BACK,		// go N bytes back

	ICONST,		// push integer to stack
	FCONST,		// push float to stack
	ILOAD,		// push int var to stack
	FLOAD,		// push float var to stack
	ISTORE,		// pop int from stack to var
	FSTORE,		// pop float from stack to var

	POP,		// pop from stack
	DUP,		// duplicate top of stack

	IADD,		// add 2 integers from stack
	FADD,		// add 2 floats from stack
	ISUB,		// sub 2 integers from stack
	FSUB,		// sub 2 floats from stack
	IMUL,		// mul 2 integers from stack
	FMUL,		// mul 2 floats from stack
	IDIV,		// div 2 integers from stack
	FDIV,		// div 2 floats from stack
	ILE,		// integer less
	IGE,		// integer greater
	FLE,		// float less
	FGE,		// float greater

	IRET,		// return integer
	FRET,		// return float

	IF,
	ELSE,
	ENDIF,
	WHILE,

	FTOI,		// float to int
	ITOF,		// int to float

	FUNCTION,
	FUNCTIONCALL,	// funccall
	FUNCS_BEGIN,
	FUNCS_END,
	ENDFUNC,
};
inline std::underlying_type_t<InstructionCode> GetCode(InstructionCode c) {
	return static_cast<std::underlying_type_t<InstructionCode>>(c);
}

void GenerateBytecode(std::ostream &out, Statement &stmt, Parser &parser);
void PrintBytecode(std::istream &in);
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <chrono>
#include "tokenizer.hpp"
#include "parser.hpp"
#include "bytecode.h"
#include "interpreter.h"

int main(int argc, char** argv) {
	std::stringstream ss{};
	std::ifstream fp("testcode.c");
	std::string tmp;
	while (std::getline(fp, tmp)) {
		ss << tmp << '\n';
	}

	auto parser = Parser(ss.str());
	
	parser.Parse();
	std::fstream file("tmp.bin", std::ios_base::binary | std::ios_base::out);
	GenerateBytecode(file, parser.GetGlobalScope().block, parser);
	file.close();

	file.open("tmp.bin", std::ios_base::binary | std::ios_base::in);
	PrintBytecode(file);
	file.clear();
	file.seekg(0, std::ios::beg);
	

	std::cout << "Interp returned: ";
	auto timeStart = std::chrono::high_resolution_clock::now();
	std::cout << InterpretCode(file);
	auto timeEnd = std::chrono::high_resolution_clock::now();
	std::cout << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count() << "ms\n";

	file.close();
	return 0;
}
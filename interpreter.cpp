#include "interpreter.h"
#include <stack>
#include <vector>
#include <deque>
#include <unordered_map>

namespace {
	using VarType = std::variant<int, float>;
	std::stack<VarType> stack;
	std::deque<VarType> vars;
	std::vector<std::string> functionDecls;

	class CustomIStream {
	private:
		std::vector<std::byte> bytes;
		decltype(bytes)::size_type readIdx;
	public:
		CustomIStream(std::vector<std::byte> &&inArr) : bytes{ inArr }, readIdx{ 0 } {}

		template<typename T>
		void read(T &toModify, std::size_t size = sizeof(T)) {
			std::memcpy(&toModify, bytes.data() + readIdx, size);
			readIdx += size;
		}
		int get() {
			return eof() ? EOF : static_cast<int>(bytes[readIdx++]);
		}
		int peek() const {
			return eof() ? EOF : static_cast<int>(bytes[readIdx]);
		}
		void reset() {
			readIdx = 0;
		}
		bool eof() const {
			return readIdx >= bytes.size();
		}

		void skip(std::uint32_t bytes) {
			readIdx += bytes;
		}
		void back(std::uint32_t bytes) {
			if (bytes > readIdx) { readIdx = 0; }
			else { readIdx -= bytes; }
		}
	};

	std::unordered_map<std::string, CustomIStream> functionBytecodes;

	std::string currFunc = "";
}

std::optional<VarType> RunCode(const std::string &func) {
	auto funcIt = functionBytecodes.find(func);
	if (funcIt == functionBytecodes.end()) return std::nullopt;
	
	auto &in = funcIt->second;

	InstructionCode code = InstructionCode::NOP;
	while (true) {
		if (in.eof()) {
			break;
		}

		in.read(code);
		switch (code)
		{
			case InstructionCode::SKIP: {
				std::uint32_t skipBytes = 0;
				in.read(skipBytes);

				in.skip(skipBytes);
				break;
			}
			case InstructionCode::BACK: {
				std::uint32_t backBytes = 0;
				in.read(backBytes);

				in.back(backBytes);
				break;
				break;
			}
			case InstructionCode::POP: {
				stack.pop();
				break;
			}
			case InstructionCode::DUP: {
				stack.push(stack.top());
				break;
			}

			case InstructionCode::ICONST:
			case InstructionCode::FCONST: {
				bool floating = code == InstructionCode::FCONST;
				VarType constant;
				if (floating) {
					float tmp;
					in.read(tmp);

					constant = tmp;
				}
				else {
					int tmp;
					in.read(tmp);

					constant = tmp;
				}

				stack.push(constant);
				break;
			}
			case InstructionCode::ILOAD:
			case InstructionCode::FLOAD: {
				std::uint32_t idx;

				in.read(idx);
				stack.push(vars[idx]);

				break;
			}
			case InstructionCode::ISTORE:
			case InstructionCode::FSTORE: {
				std::uint32_t idx;
				in.read(idx);

				if (idx + 1 > vars.size()) {
					vars.resize(idx + 1);
				}

				vars[idx] = stack.top();
				stack.pop();

				break;
			}
			
			case InstructionCode::IADD:
			case InstructionCode::FADD: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FCONST;

				if (floating) {
					stack.push(std::get<float>(a) + std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) + std::get<int>(b));
				}

				break;
			}
			case InstructionCode::ISUB:
			case InstructionCode::FSUB: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FCONST;

				if (floating) {
					stack.push(std::get<float>(a) - std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) - std::get<int>(b));
				}

				break;
			}
			case InstructionCode::IMUL:
			case InstructionCode::FMUL: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FCONST;

				if (floating) {
					stack.push(std::get<float>(a) * std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) * std::get<int>(b));
				}

				break;
			}
			case InstructionCode::IDIV:
			case InstructionCode::FDIV: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FCONST;

				if (floating) {
					stack.push(std::get<float>(a) / std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) / std::get<int>(b));
				}

				break;
			}
			case InstructionCode::IGE:
			case InstructionCode::FGE: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FGE;

				if (floating) {
					stack.push(std::get<float>(a) > std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) > std::get<int>(b));
				}

				break;
			}
			case InstructionCode::ILE:
			case InstructionCode::FLE: {
				VarType b = stack.top(); stack.pop();
				VarType a = stack.top(); stack.pop();
				bool floating = code == InstructionCode::FGE;

				if (floating) {
					stack.push(std::get<float>(a) < std::get<float>(b));
				}
				else {
					stack.push(std::get<int>(a) < std::get<int>(b));
				}

				break;
			}

			case InstructionCode::IRET:
			case InstructionCode::FRET: {
				auto top = stack.top();
				stack.pop();

				if(code == InstructionCode::IRET)
					return std::get<int>(top);
				else
					return std::get<float>(top);
			}
			
			case InstructionCode::FTOI: {
				auto var = stack.top();
				stack.pop();
				stack.push(static_cast<int>(std::get<float>(var)));
				break;
			}
			case InstructionCode::ITOF: {
				auto var = stack.top();
				stack.pop();
				stack.push(static_cast<float>(std::get<int>(var)));
				break;
			}

			case InstructionCode::FUNCTIONCALL: {
				std::string funcSig = "";
				while (in.peek() != '\0') {
					funcSig += (char)in.get();
				}
				in.get();

				auto var = RunCode(funcSig);
				if (var) {
					stack.push(var.value());
				}
				break;
			}
			
			case InstructionCode::IF: {
				std::uint32_t skipIfFalse{};
				in.read(skipIfFalse);

				auto compRes = stack.top();
				stack.pop();
				if ((std::get_if<int>(&compRes) && std::get<int>(compRes)) || (std::get_if<float>(&compRes) && std::get<float>(compRes))) {
					break;
				}

				in.skip(skipIfFalse);

				break;
			}
			case InstructionCode::WHILE: {
				std::uint32_t skipBytes = 0;
				in.read(skipBytes);

				auto condVal = stack.top(); stack.pop();
				if (!((std::get_if<int>(&condVal) && std::get<int>(condVal)) || (std::get_if<float>(&condVal) && std::get<float>(condVal)))) {
					in.skip(skipBytes);
					break;
				}
				break;
			}
			default:
				break;
		}
	}
	return std::nullopt;
}

int InterpretCode(std::istream &in) {
	while (!in.eof()) {
		InstructionCode code;
		in.read(reinterpret_cast<char *>(&code), sizeof(code));
		switch (code) {
			case InstructionCode::FUNCS_BEGIN: {
				do {
					std::string temporary;
					std::getline(in, temporary);
					functionDecls.push_back(temporary);

					code = static_cast<InstructionCode>(in.peek());
				} while (code != InstructionCode::FUNCS_END);
				in.get();
				break;
			}
			case InstructionCode::FUNCTION: {
				std::string funcName = "";
				std::vector<std::byte> bytes;
				while (in.peek() != '\0') {
					funcName += in.get();
				}

				in.get();

				while (true) {
					if (in.eof()) {
						break;
					}

					if (in.peek() == GetCode(InstructionCode::ENDFUNC)) {
						in.get();
						if (in.peek() == GetCode(InstructionCode::FUNCTION)) {
							break;
						}
					}
					bytes.push_back(static_cast<std::byte>(in.get()));
				}
				functionBytecodes.insert(std::pair<std::string, CustomIStream>(funcName, CustomIStream{ std::move(bytes) }));

				break;
			}
		}
	}

	auto var = RunCode("main()");

	if (!var) return -1;
	return std::get<int>(var.value());
}
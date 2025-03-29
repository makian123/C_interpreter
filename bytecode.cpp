#include "bytecode.h"
#include <stack>
#include <iostream>
#include <unordered_map>

void GenerateBytecode(std::ostream &out, Statement &stmt);

namespace {
	using UnfinishedBreak = std::uint32_t;
	Parser *globalParser = nullptr;
	Scope *currScope = nullptr;
	std::uint32_t varIdx = 0, currFuncIdx = 0;
	std::stack<std::unordered_map<std::string, std::pair<std::uint32_t, VarDeclStmt*>>> vars;

	// Loop stuff
	std::stack<std::uint32_t> loopBeginBytes;
	std::stack<std::vector<UnfinishedBreak>> unfinishedBreaks;
	std::stack<Statement *> postLoopStatements;
	
	std::uint32_t GetVariableIdx(const std::string &toFind) {
		if (!vars.size()) {
			return -1;
		}
		for (const auto &[var, idx] : vars.top()) {
			if (var == toFind) {
				return idx.first;
			}
		}
		decltype(vars)::value_type tmp = vars.top();
		vars.pop();

		auto ret = GetVariableIdx(toFind);
		vars.push(std::move(tmp));
		return ret;
	}
	VarDeclStmt *GetVariableStmt(std::uint32_t toFind) {
		if (!vars.size()) {
			return nullptr;
		}
		for (const auto &[var, idx] : vars.top()) {
			if (idx.first == toFind) {
				return idx.second;
			}
		}
		decltype(vars)::value_type tmp = vars.top();
		vars.pop();

		auto ret = GetVariableStmt(toFind);
		vars.push(std::move(tmp));
		return ret;
	}

	void GenerateExprBytecode(std::ostream &out, Expression &expr);

	void GenerateValueBytecode(std::ostream &out, ValueExpr &expr) {
		bool floating = false;
		switch (expr.val.type) {
			case TokenType::INTEGER:
				out << GetCode(InstructionCode::ICONST);
				break;
			case TokenType::FLOAT:
				out << GetCode(InstructionCode::FCONST);
				floating = true;
				break;
			case TokenType::IDENT: {
				auto varIdx = GetVariableIdx(expr.val.value);
				bool floating = GetVariableStmt(varIdx)->var.type->name.value == "float";

				out << GetCode(floating ? InstructionCode::FLOAD : InstructionCode::ILOAD);
				out.write(reinterpret_cast<char*>(&varIdx), sizeof(varIdx));
				return;
			}
		}

		if (floating) {
			auto var = std::stof(expr.val.value);
			out.write(reinterpret_cast<char *>(&var), sizeof(var));
		}
		else {
			auto var = std::stoi(expr.val.value);
			out.write(reinterpret_cast<char *>(&var), sizeof(var));
		}
	}
	void GenerateBinaryBytecode(std::ostream &out, BinaryExpression &expr) {
		GenerateExprBytecode(out, *expr.lhs.get());
		GenerateExprBytecode(out, *expr.rhs.get());

		const auto *evaluatedType = globalParser->EvalType(expr, currScope);
		bool floating = evaluatedType->name.value == "float";
		switch (expr.op.type) {
		case TokenType::PLUS:
			out << GetCode(floating ? InstructionCode::FADD : InstructionCode::IADD);
			break;
		case TokenType::MINUS:
			out << GetCode(floating ? InstructionCode::FSUB : InstructionCode::ISUB);
			break;
		case TokenType::STAR:
			out << GetCode(floating ? InstructionCode::FMUL : InstructionCode::IMUL);
			break;
		case TokenType::SLASH:
			out << GetCode(floating ? InstructionCode::FDIV : InstructionCode::IDIV);
			break;
		case TokenType::PERCENT:
			out << GetCode(InstructionCode::MOD);
			break;
		case TokenType::EQUALS:
			out << GetCode(floating ? InstructionCode::FEQ : InstructionCode::IEQ);
			break;
		case TokenType::LESS:
			out << GetCode(floating ? InstructionCode::FLE : InstructionCode::ILE);
			break;
		case TokenType::GREATER:
			out << GetCode(floating ? InstructionCode::FGE : InstructionCode::IGE);
			break;
		}
	}
	void GenerateUnaryBytecode(std::ostream &out, UnaryExpr &expr) {
		out << GetCode(expr.op.type == TokenType::INCREMENT ? InstructionCode::INC : InstructionCode::DEC);
		std::uint32_t variableIndex = GetVariableIdx(expr.expr->val.value);
		out.write(reinterpret_cast<char *>(&variableIndex), sizeof(variableIndex));
	}
	void GenerateCastBytecode(std::ostream &out, CastExpr &expr) {
		GenerateExprBytecode(out, *expr.expr.get());
		if (expr.finalType == expr.origType) {
			return;
		}

		out << GetCode(expr.finalType->name.value == "float" ? InstructionCode::ITOF : InstructionCode::FTOI);
	}
	void GenerateFunccallBytecode(std::ostream &out, FuncCallExpr &expr) {
		for (auto &param: expr.params) {
			GenerateExprBytecode(out, *param);
		}
		
		out << GetCode(InstructionCode::FUNCTIONCALL);
		std::string funcSig = currScope->FindFunc(expr.func)->GenerateSignature() + '\n';
		out << funcSig;//(funcSig.data(), funcSig.size());
		const std::uint32_t paramSz = expr.params.size();
		out.write(reinterpret_cast<const char *>(&paramSz), sizeof(paramSz));
	}
	void GenerateExprBytecode(std::ostream &out, Expression &expr) {
		switch (expr.type) {
			case ExpressionType::VALUE:
				GenerateValueBytecode(out, static_cast<ValueExpr &>(expr));
				break;
			case ExpressionType::BINARY:
				GenerateBinaryBytecode(out, static_cast<BinaryExpression &>(expr));
				break;
			case ExpressionType::UNARY:
				GenerateUnaryBytecode(out, static_cast<UnaryExpr &>(expr));
				break;
			case ExpressionType::CAST:
				GenerateCastBytecode(out, static_cast<CastExpr &>(expr));
				break;
			case ExpressionType::FUNCCALL:
				GenerateFunccallBytecode(out, static_cast<FuncCallExpr &>(expr));
				break;
		}
	}

	void GenerateIfBytecode(std::ostream &out, IfStmt &stmt) {
		GenerateExprBytecode(out, *stmt.condition.get());
		out << GetCode(InstructionCode::IF);

		// To skip if false
		std::uint32_t offset = 0;
		out.write(reinterpret_cast<char *>(&offset), sizeof(offset));
		auto position = out.tellp();

		vars.emplace();
		GenerateBytecode(out, *stmt.then);
		vars.pop();

		// Skip else when finished
		out << GetCode(InstructionCode::SKIP);
		out.write(reinterpret_cast<char *>(&offset), sizeof(offset));
		int ifEndPos = out.tellp();

		// Skip if false offset
		int ifFinishedPos = out.tellp();
		std::streamoff skipToElse = offset = (ifFinishedPos - position);
		out.seekp(-skipToElse - sizeof(offset), out.cur);
		out.write(reinterpret_cast<char*>(&offset), sizeof(offset));
		out.seekp(skipToElse, out.cur);

		if (stmt.els) {
			out << GetCode(InstructionCode::ELSE);
			vars.emplace();
			GenerateBytecode(out, *stmt.els);
			vars.pop();
		}

		int ifStmtEnd = out.tellp();
		int skipElse = offset = (ifStmtEnd - ifEndPos);
		out.seekp(ifEndPos - sizeof(offset), out.beg);
		out.write(reinterpret_cast<char*>(&offset), sizeof(offset));

		out.seekp(ifStmtEnd, out.beg);
	}
	void GenerateWhileBytecode(std::ostream &out, WhileStmt &stmt) {
		std::uint32_t endWhileOff = 0;

		int whileStartPos = out.tellp();
		loopBeginBytes.push(whileStartPos);
		unfinishedBreaks.emplace();
		GenerateExprBytecode(out, *stmt.condition);

		out << GetCode(InstructionCode::WHILE);
		int whileSkipPos = out.tellp();
		out.write(reinterpret_cast<char *>(&endWhileOff), sizeof(endWhileOff));

		vars.emplace();
		GenerateBytecode(out, *stmt.then);
		vars.pop();

		out << GetCode(InstructionCode::BACK);
		std::uint32_t backBytes = (int)out.tellp() - whileStartPos + sizeof(endWhileOff);
		out.write(reinterpret_cast<char *>(&backBytes), sizeof(backBytes));

		int loopEnd = out.tellp();
		endWhileOff = (loopEnd - whileSkipPos) - sizeof(endWhileOff);
		out.seekp(whileSkipPos, out.beg);
		out.write(reinterpret_cast<char *>(&endWhileOff), sizeof(endWhileOff));

		for (const auto &unfinishedBreak : unfinishedBreaks.top()) {
			std::uint32_t offset = loopEnd - unfinishedBreak - sizeof(uint32_t);
			out.seekp(unfinishedBreak, out.beg);
			out.write(reinterpret_cast<char *>(&offset), sizeof(offset));
		}

		out.seekp(0, out.end);
		unfinishedBreaks.pop();
		loopBeginBytes.pop();
	}
	void GenerateForBytecode(std::ostream &out, ForStmt &stmt) {
		std::uint32_t offset = 0;

		vars.emplace();
		unfinishedBreaks.emplace();
		postLoopStatements.push(stmt.postLoop.get());
		GenerateBytecode(out, *stmt.initial);

		int conditionPos = out.tellp();
		loopBeginBytes.push(conditionPos);
		GenerateExprBytecode(out, *stmt.condition);

		out << GetCode(InstructionCode::FOR);
		auto offsetPos = out.tellp();
		out.write(reinterpret_cast<char *>(&offset), sizeof(offset));

		GenerateBytecode(out, *stmt.then);
		GenerateBytecode(out, *stmt.postLoop);

		out << GetCode(InstructionCode::BACK);
		// Go back to start of loop (conditions)
		offset = (int)out.tellp() - conditionPos + sizeof(offset);
		out.write(reinterpret_cast<char*>(&offset), sizeof(offset));

		// Skip loop size in bytes
		int loopEnd = out.tellp();
		offset = loopEnd - offsetPos - sizeof(offset);
		out.seekp(offsetPos, out.beg);
		out.write(reinterpret_cast<char *>(&offset), sizeof(offset));

		for (const auto &unfinishedBreak : unfinishedBreaks.top()) {
			std::uint32_t offset = loopEnd - unfinishedBreak - sizeof(uint32_t);
			out.seekp(unfinishedBreak, out.beg);
			out.write(reinterpret_cast<char *>(&offset), sizeof(offset));
		}

		out.seekp(0, out.end);
		unfinishedBreaks.pop();
		vars.pop();
		loopBeginBytes.pop();
		postLoopStatements.pop();
	}
	void GenerateContinueBytecode(std::ostream &out, ContinueStmt &stmt) {
		if (postLoopStatements.size() && postLoopStatements.top()) {
			GenerateBytecode(out, *postLoopStatements.top());
		}
		out << GetCode(InstructionCode::BACK);
		int backPos = out.tellp();
		std::uint32_t backbytes = backPos - loopBeginBytes.top() + sizeof(uint32_t);
		out.write(reinterpret_cast<char *>(&backbytes), sizeof(backbytes));
	}
	void GenerateBreakBytecode(std::ostream &out, BreakStmt &stmt) {
		out << GetCode(InstructionCode::SKIP);
		std::uint32_t skipCnt = 0;
		unfinishedBreaks.top().push_back(out.tellp());
		out.write(reinterpret_cast<char *>(&skipCnt), sizeof(skipCnt));
	}
	void GenerateBlockBytecode(std::ostream &out, BlockStmt &stmt) {
		for (auto &stmt_child : stmt.stmts) {
			GenerateBytecode(out, *stmt_child.get());
		}
	}
	void GenerateFuncBytecode(std::ostream &out, FuncDeclStmt &stmt) {
		currScope = currScope->children[currFuncIdx++].get();
		vars.push(decltype(vars)::value_type{});

		for (auto &param : stmt.params) {
			vars.top()[param->var.name.value] = std::pair<std::uint32_t, VarDeclStmt *>(varIdx++, param.get());
		}

		out << GetCode(InstructionCode::FUNCTION);
		out << currScope->FindFunc(stmt.name)->GenerateSignature();
		out.put('\n');
		GenerateBlockBytecode(out, *stmt.definition.get());
		out << GetCode(InstructionCode::ENDFUNC);

		varIdx -= vars.top().size();
		vars.pop();

		currScope = currScope->parent;
	}
	void GenerateVarDeclBytecode(std::ostream &out, VarDeclStmt &stmt) {
		GenerateExprBytecode(out, *stmt.expr);
		bool floating = stmt.var.type->name.value == "float";

		out << GetCode(floating ? InstructionCode::FSTORE : InstructionCode::ISTORE);
		out.write(reinterpret_cast<char *>(&varIdx), sizeof(varIdx));

		vars.top()[stmt.var.name.value] = std::pair<std::uint32_t, VarDeclStmt*>{varIdx++, &stmt};
	}
	void GenerateVarAssignBytecode(std::ostream &out, VarAssignStmt &stmt) {
		GenerateExprBytecode(out, *stmt.val);
		auto idx = GetVariableIdx(stmt.name.value);
		bool floating = GetVariableStmt(idx)->var.type->name.value == "float";

		out << GetCode(floating ? InstructionCode::FSTORE : InstructionCode::ISTORE);
		out.write(reinterpret_cast<char *>(&idx), sizeof(idx));
	}
	void GenerateReturnBytecode(std::ostream &out, ReturnStmt &stmt) {
		GenerateExprBytecode(out, *stmt.ret.get());
		out << GetCode(InstructionCode::IRET);
	}

	union Constant {
		int integer;
		float floating;
	};
}

void GenerateBytecode(std::ostream &out, Statement &stmt) {
	switch (stmt.type) {
	case StatementType::BLOCK:
		GenerateBlockBytecode(out, static_cast<BlockStmt &>(stmt));
		break;
	case StatementType::FUNCDECL:
		GenerateFuncBytecode(out, static_cast<FuncDeclStmt &>(stmt));
		break;
	case StatementType::VARDECL:
		GenerateVarDeclBytecode(out, static_cast<VarDeclStmt &>(stmt));
		break;
	case StatementType::VARASSIGN:
		GenerateVarAssignBytecode(out, static_cast<VarAssignStmt &>(stmt));
		break;
	case StatementType::RETURN:
		GenerateReturnBytecode(out, static_cast<ReturnStmt&>(stmt));
		break;
	case StatementType::IF:
		GenerateIfBytecode(out, static_cast<IfStmt &>(stmt));
		break;
	case StatementType::WHILE:
		GenerateWhileBytecode(out, static_cast<WhileStmt &>(stmt));
		break;
	case StatementType::FOR:
		GenerateForBytecode(out, static_cast<ForStmt &>(stmt));
		break;
	case StatementType::BREAK:
		GenerateBreakBytecode(out, static_cast<BreakStmt &>(stmt));
		break;
	case StatementType::CONTINUE:
		GenerateContinueBytecode(out, static_cast<ContinueStmt &>(stmt));
		break;
	case StatementType::EXPRSTMT:
		GenerateExprBytecode(out, *static_cast<ExpressionStmt &>(stmt).expr);
		break;
	}
}
void GenerateBytecode(std::ostream &out, Statement &stmt, Parser &parser) {
	globalParser = &parser;
	currScope = &globalParser->GetGlobalScope();
	vars.push(decltype(vars)::value_type{});

	out << GetCode(InstructionCode::FUNCS_BEGIN);
	for (auto &func : parser.GetGlobalScope().funcs) {
		out << func->GenerateSignature() << '\n';
	}
	out << GetCode(InstructionCode::FUNCS_END);

	GenerateBytecode(out, stmt);
	globalParser = nullptr;
}

void PrintNextBytecode(std::istream &in) {
	InstructionCode code;
	std::cout << in.tellg() << ": ";
	in.read(reinterpret_cast<char *>(&code), sizeof(InstructionCode));
	switch (code) {
		case InstructionCode::SKIP: {
			std::uint32_t skipBytes = 0;
			in.read(reinterpret_cast<char *>(&skipBytes), sizeof(skipBytes));
			std::cout << "SKIP " << skipBytes << " bytes\n";
			break;
		}
		case InstructionCode::BACK: {
			std::uint32_t backBytes = 0;
			in.read(reinterpret_cast<char *>(&backBytes), sizeof(backBytes));
			std::cout << "BACK " << backBytes << " bytes\n";
			break;
		}
		case InstructionCode::DUP: {
			std::cout << "DUP\n";
			break;
		}
		case InstructionCode::POP: {
			std::cout << "POP\n";
			break;
		}

		case InstructionCode::ICONST:
		case InstructionCode::FCONST: {
			Constant num;
			in.read(reinterpret_cast<char *>(&num), sizeof(int));
			std::cout << "PUSH " << (code == InstructionCode::ICONST ? num.integer : num.floating) << '\n';
			break;
		}
		
		case InstructionCode::ILOAD:
		case InstructionCode::FLOAD: {
			std::uint32_t var;
			in.read(reinterpret_cast<char *>(&var), sizeof(var));
			std::cout << "PUSH FROM #" << var << '\n';
			break;
		}
		
		case InstructionCode::ISTORE:
		case InstructionCode::FSTORE: {
			std::uint32_t var;
			in.read(reinterpret_cast<char *>(&var), sizeof(var));
			std::cout << "STORE INTO #" << var << '\n';
			break;
		}

		case InstructionCode::IADD:
		case InstructionCode::FADD: {
			std::cout << "ADD\n";
			break;
		}
		case InstructionCode::ISUB:
		case InstructionCode::FSUB: {
			std::cout << "SUB\n";
			break;
		}
		case InstructionCode::IMUL:
		case InstructionCode::FMUL: {
			std::cout << "MUL\n";
			break;
		}
		case InstructionCode::IDIV:
		case InstructionCode::FDIV: {
			std::cout << "DIV\n";
			break;
		}
		case InstructionCode::INC:
		
		case InstructionCode::DEC: {
			std::uint32_t varIdx = 0;
			in.read(reinterpret_cast<char*>(&varIdx), sizeof(varIdx));
			std::cout << (code == InstructionCode::INC ? "INC" : "DEC") << " #" << varIdx << '\n';

			break;
		}
		case InstructionCode::ILE:
		case InstructionCode::FLE: {
			std::cout << "LESS\n";
			break;
		}
		case InstructionCode::IGE:
		case InstructionCode::FGE: {
			std::cout << "GREATER\n";
			break;
		}
		case InstructionCode::IEQ:
		case InstructionCode::FEQ: {
			std::cout << "EQUALS\n";
			break;
		}

		case InstructionCode::IF: {
			std::uint32_t codeSz = 0;
			in.read(reinterpret_cast<char *>(&codeSz), sizeof(codeSz));
			std::cout << "IF (skip " << codeSz << " bytes)\n";
			break;
		}
		case InstructionCode::ELSE: {
			std::cout << "ELSE\n";
			break;
		}
		case InstructionCode::WHILE: {
			std::uint32_t codeSz = 0;
			in.read(reinterpret_cast<char *>(&codeSz), sizeof(codeSz));
			std::cout << "WHILE (skip " << codeSz << " bytes)\n";
			break;
		}
		case InstructionCode::FOR: {
			std::uint32_t codeSz = 0;
			in.read(reinterpret_cast<char *>(&codeSz), sizeof(codeSz));
			std::cout << "FOR (skip " << codeSz << " bytes)\n";
			break;
		}

		case InstructionCode::FUNCTION: {
			std::string funcSig;
			while (in.peek() != '\n') {
				funcSig += in.get();
			}
			int ch = in.get();

			std::cout << funcSig << ":\n";

			while (!in.eof() && (ch = in.peek()) != '}') {
				PrintNextBytecode(in);
			}
			in.get();
			break;
		}
		case InstructionCode::ENDFUNC: {
			std::cout << "ENDFUNC\n";
			break;
		}
		case InstructionCode::FUNCTIONCALL: {
			std::cout << "CALL ";
			while (in.peek() != '\n') {
				std::cout << (char)in.get();
			}
			in.get();
			std::uint32_t params = 0;
			in.read(reinterpret_cast<char *>(&params), sizeof(params));
			std::cout << " (" << params << ") params\n";
			break;
		}
		case InstructionCode::IRET:
		case InstructionCode::FRET: {
			std::cout << "RETURN\n";
			break;
		}

		case InstructionCode::FTOI:
		case InstructionCode::ITOF: {
			std::cout << "CAST TO " << (code == InstructionCode::FTOI ? "INT" : "FLOAT") << "\n";
			break;
		}

		case InstructionCode::FUNCS_BEGIN: {
			std::string tmp;

			std::cout << "FUNCS\n";
			while (true) {
				std::getline(in, tmp);
				std::cout << tmp << '\n';
				if (in.peek() == GetCode(InstructionCode::FUNCS_END)) {
					std::cout << "ENDFUNCS\n\n";
					in.get();
					break;
				}
			}
			break;
		}
	}
}
void PrintBytecode(std::istream &in) {
	while (!in.eof()) {
		PrintNextBytecode(in);
	}
}
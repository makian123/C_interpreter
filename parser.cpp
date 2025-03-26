#include "parser.hpp"

#undef NDEBUG
#include <cassert>

namespace {
	int Precedence(TokenType tok) {
		using Type = TokenType;

		switch (tok) {
		case Type::STAR:
		case Type::SLASH:
			return 3;
		case Type::PLUS:
		case Type::MINUS:
			return 2;
		case Type::LESS:
		case Type::GREATER:
			return 1;
		/*case Type::LESS:
		case Type::LEQ:
		case Type::GREATER:
		case Type::GEQ:
		case Type::NEQ:
			return 1;*/
		}
		return 0;
	}
	bool ImplicitlyCastable(const Type *orig, const Type *dest) {
		const Type::Structure *a = std::get_if<Type::Structure>(&orig->optionalData);
		const Type::Structure *b = std::get_if<Type::Structure>(&orig->optionalData);

		// Cant implicit cast different structures
		if (a || b) {
			return false;
		}

		return true;
	}
}

std::string Function::GenerateSignature() const {
	std::string out = name.value + "(";
	for (auto it = params.begin(); it != params.end(); ++it) {
		out += it->type->name.value;
		if (it != params.end() - 1) {
			out += ',';
		}
	}
	return out + ")";
}

std::unique_ptr<Expression> Parser::ParsePrimaryExpr() {
	if (tokenizer.Get().type == TokenType::INTEGER || tokenizer.Get().type == TokenType::FLOAT) {
		return std::make_unique<ValueExpr>(tokenizer.Next());
	}
	else if (tokenizer.Get().type == TokenType::IDENT) {
		auto name = tokenizer.Next();
		auto beginIdx = tokenizer.GetIdx();

		// If function
		if (tokenizer.Get().type == TokenType::OPEN_PARENTH) {
			tokenizer.Next();
			std::vector<std::unique_ptr<Expression>> params;

			if (tokenizer.Get().type == TokenType::CLOSED_PARENTH) {
				tokenizer.Next();
				assert(currentScope->FindFunc(name));
				return std::make_unique<FuncCallExpr>(name, std::move(params));
			}

			if (name.value == "sizeof") {
				auto typeSz = GetType()->size;
				assert(tokenizer.Next().type == TokenType::CLOSED_PARENTH);
				return std::make_unique<ValueExpr>(Token{TokenType::INTEGER, 0, 0, std::to_string(typeSz)});
			}

			while (true) {
				auto expr = ParseExpr();
				assert(expr);

				params.emplace_back(std::move(expr));
				if (tokenizer.Get().type == TokenType::COMMA) {
					tokenizer.Next();
					continue;
				}

				if (tokenizer.Get().type != TokenType::CLOSED_PARENTH) {
					return nullptr;
				}

				tokenizer.Next();
				break;
			}

			auto func = currentScope->FindFunc(name);
			assert(func && (func->params.size() == params.size()));
			for (std::size_t i = 0; i < func->params.size(); ++i) {
				auto *type = EvalType(*params[i].get());
				assert(type && ImplicitlyCastable(type, func->params[i].type));

				// Cast
				if (!type->operator==(*func->params[i].type)) {
					params[i] = std::make_unique<CastExpr>(type, func->params[i].type, std::move(params[i]));
				}
			}

			assert(currentScope->FindFunc(name));
			return std::make_unique<FuncCallExpr>(name, std::move(params));
		}
		// Member
		else if (tokenizer.Get().type == TokenType::DOT) {

		}
		else {
			assert(currentScope->FindVar(name));
			return std::make_unique<ValueExpr>(name);
		}
		return nullptr;
	}
	else if (tokenizer.Get().type == TokenType::OPEN_PARENTH) {
		tokenizer.Next();
		if (tokenizer.Get().type == TokenType::TYPE_STRUCT) {
			tokenizer.Next();
		}

		auto *finalType = currentScope->FindType(tokenizer.Next());
		assert(tokenizer.Next().type == TokenType::CLOSED_PARENTH);
		auto expression = ParseExpr();
		auto *evaledType = EvalType(*expression.get());
		return std::make_unique<CastExpr>(evaledType, finalType, std::move(expression));
	}
	return nullptr;
}

std::unique_ptr<Expression> Parser::ParseExpr(int precedence) {
	auto left = ParsePrimaryExpr();

	while (true) {
		auto newPrece = Precedence(tokenizer.Get().type);
		if (newPrece == 0 || newPrece < precedence) break;

		auto op = tokenizer.Next();
		auto right = ParseExpr(newPrece);
		left = std::make_unique<BinaryExpression>(std::move(left), op, std::move(right));
	}

	return left;
}
std::unique_ptr<Type> Parser::ParseType() {
	assert(tokenizer.Get().type == TokenType::TYPE_STRUCT); tokenizer.Next();
	Token typeName;
	if (tokenizer.Get().type == TokenType::IDENT) {
		typeName = tokenizer.Next();
	}
	if (tokenizer.Get().type == TokenType::SEMICOLON) {
		tokenizer.Next();
		return std::make_unique<Type>(typeName, 0, 0, Type::Structure{ false, {} });
	}
	assert(tokenizer.Get().type == TokenType::OPEN_BRACE); tokenizer.Next();
	std::unique_ptr<Type> t = std::move(std::make_unique<Type>(typeName, 0, 0, Type::Structure{ true, {} }));

	while (std::unique_ptr<Statement> stmt = std::move(ParseVarDecl())) {
		auto stm = static_cast<VarDeclStmt*>(stmt.get());
		auto align = stm->var.type->alignment;
		auto off = t->size;
		off += off % align;
		std::get<Type::Structure>(t->optionalData).members.push_back({ stm->var, off });
		t->size = off + stm->var.type->size;
	}
	t->alignment = t->size;
	if (t->alignment % 2 || t->alignment > 8) {
		t->alignment = t->alignment > 8 ? 8 : (t->alignment + 1);
	}

	assert(tokenizer.Next().type == TokenType::CLOSED_BRACE);
	assert(tokenizer.Next().type == TokenType::SEMICOLON);

	return t;
}

const Type *Parser::EvalType(Expression &expr, Scope *scope) const {
	if (!scope) {
		scope = currentScope;
	}

	if (expr.type == ExpressionType::VALUE) {
		auto &cast = static_cast<ValueExpr &>(expr);
		switch (cast.val.type) {
		case TokenType::IDENT:
			return scope->FindVar(cast.val)->type;
		case TokenType::INTEGER:
			return scope->FindType({ TokenType::IDENT, 0, 0, "int" });
		case TokenType::FLOAT:
			return scope->FindType({ TokenType::IDENT, 0, 0, "double" });
		}
	}
	else if (expr.type == ExpressionType::BINARY) {
		auto &cast = static_cast<BinaryExpression &>(expr);
		auto *left = EvalType(*cast.lhs.get(), scope);
		auto *right = EvalType(*cast.rhs.get(), scope);

		assert(left && right);

		if (left->name.value != right->name.value) {
			return left->name.type == TokenType::INTEGER ? right : left;
		}
		return left;
	}
	else if (expr.type == ExpressionType::CAST) {
		return static_cast<CastExpr &>(expr).finalType;
	}
	else if (expr.type == ExpressionType::FUNCCALL) {
		auto &cast = static_cast<FuncCallExpr &>(expr);
		return scope->FindFunc(cast.func)->returnType;
	}
	return nullptr;
}

Type* Scope::FindType(Token name) {
	for (auto& t : types) {
		if (t->name.value == name.value) return t.get();
	}
	for (auto& alias : typedefs) {
		if (name.value == alias.newName.value) return FindType(alias.originalName);
	}

	if (parent) return parent->FindType(name);
	return nullptr;
}
Variable *Scope::FindVar(Token name) {
	for (auto &var : vars) {
		if (var->name.value != name.value) continue;
		return var.get();
	}

	return parent ? parent->FindVar(name) : nullptr;
}
Function *Scope::FindFunc(Token name) {
	for (auto &func : funcs) {
		if (func->name.value != name.value) continue;
		return func.get();
	}

	return parent ? parent->FindFunc(name) : nullptr;
}

const Type *Scope::FindType(Token name) const {
	for (auto &t : types) {
		if (t->name.value == name.value) return t.get();
	}
	for (auto &alias : typedefs) {
		if (name.value == alias.newName.value) return FindType(alias.originalName);
	}

	if (parent) return parent->FindType(name);
	return nullptr;
}
const Variable *Scope::FindVar(Token name) const{
	for (auto &var : vars) {
		if (var->name.value != name.value) continue;
		return var.get();
	}

	return parent ? parent->FindVar(name) : nullptr;
}
const Function *Scope::FindFunc(Token name) const{
	if (parent) {
		return parent->FindFunc(name);
	}
	for (auto &func : funcs) {
		if (func->name.value != name.value) continue;
		return func.get();
	}
	return nullptr;
}

std::unique_ptr<VarDeclStmt> Parser::ParseVarDecl(bool isParam){
	Token typeName = tokenizer.Get();
	auto type = currentScope->FindType(typeName);
	if(!type) return nullptr; tokenizer.Next();

	Token varName = tokenizer.Next();
	if(varName.type != TokenType::IDENT) return nullptr;
	assert(currentScope->FindVar(varName) == nullptr);

	std::unique_ptr<Expression> expr = nullptr;
	if (tokenizer.Get().type != TokenType::SEMICOLON && !isParam) {
		tokenizer.Next();
		expr = ParseExpr();
	}

	if (!isParam) {
		assert(tokenizer.Next().type == TokenType::SEMICOLON);
		currentScope->vars.push_back(std::make_unique<Variable>(type, varName));
	}
	return std::make_unique<VarDeclStmt>(varName, type, static_cast<Modifiers>(0), std::move(expr));
}
std::unique_ptr<VarAssignStmt> Parser::ParseVarAssign() {
	auto varName = tokenizer.Next();
	assert(varName.type == TokenType::IDENT);
	assert(tokenizer.Next().type == TokenType::ASSIGN);

	auto ret = std::make_unique<VarAssignStmt>(varName, std::move(ParseExpr()));
	assert(tokenizer.Next().type == TokenType::SEMICOLON);

	return ret;
}
std::unique_ptr<IfStmt> Parser::ParseIf() {
	assert(tokenizer.Get().type == TokenType::IF); tokenizer.Next();
	assert(tokenizer.Get().type == TokenType::OPEN_PARENTH); tokenizer.Next();

	auto expr{ ParseExpr() };
	assert(expr);

	assert(tokenizer.Get().type == TokenType::CLOSED_PARENTH); tokenizer.Next();

	std::unique_ptr<Statement> then{ nullptr };
	std::unique_ptr<Statement> els{ nullptr };

	if (tokenizer.Get().type != TokenType::OPEN_BRACE) {
		then = std::move(ParseStmt());
	}
	else {
		tokenizer.Next();

		currentScope->children.emplace_back(std::make_unique<Scope>());
		currentScope->children.back()->parent = currentScope;
		currentScope = currentScope->children.back().get();

		then = std::move(ParseBlock());
		assert(tokenizer.Get().type == TokenType::CLOSED_BRACE); tokenizer.Next();

		currentScope = currentScope->parent;
	}

	// No else, then exit with just then
	if (tokenizer.Get().type != TokenType::ELSE) {
		return std::make_unique<IfStmt>(std::move(expr), std::move(then), std::move(els));
	}
	tokenizer.Next();

	if (tokenizer.Get().type != TokenType::OPEN_BRACE) {
		els = std::move(ParseStmt());
	}
	else {
		tokenizer.Next();

		currentScope->children.emplace_back(std::make_unique<Scope>());
		currentScope->children.back()->parent = currentScope;
		currentScope = currentScope->children.back().get();

		els = std::move(ParseBlock());
		assert(tokenizer.Get().type == TokenType::CLOSED_BRACE); tokenizer.Next();

		currentScope = currentScope->parent;
	}

	return std::make_unique<IfStmt>(std::move(expr), std::move(then), std::move(els));
}
std::unique_ptr<WhileStmt> Parser::ParseWhile() {
	assert(tokenizer.Get().type == TokenType::WHILE); tokenizer.Next();
	assert(tokenizer.Get().type == TokenType::OPEN_PARENTH); tokenizer.Next();

	auto expr{ ParseExpr() };
	assert(expr);

	assert(tokenizer.Get().type == TokenType::CLOSED_PARENTH); tokenizer.Next();

	std::unique_ptr<Statement> then{ nullptr };
	if (tokenizer.Get().type != TokenType::OPEN_BRACE) {
		then = std::move(ParseStmt());
	}
	else {
		tokenizer.Next();

		currentScope->children.emplace_back(std::make_unique<Scope>());
		currentScope->children.back()->parent = currentScope;
		currentScope = currentScope->children.back().get();

		then = std::move(ParseBlock());
		assert(tokenizer.Get().type == TokenType::CLOSED_BRACE); tokenizer.Next();

		currentScope = currentScope->parent;
	}

	return std::make_unique<WhileStmt>(std::move(expr), std::move(then));
}
std::unique_ptr<BlockStmt> Parser::ParseBlock() {
	std::unique_ptr<BlockStmt> ret = std::make_unique<BlockStmt>();
	currentScope->children.push_back(std::make_unique<Scope>());
	currentScope->children.back()->parent = currentScope;
	currentScope = currentScope->children.back().get();

	std::unique_ptr<Statement> stmt;
	while (stmt = ParseStmt()) {
		if (stmt->type == StatementType::VARDECL || stmt->type == StatementType::VARASSIGN) {
			//assert(tokenizer.Next().type == TokenType::SEMICOLON);
		}
		ret->AddStmt(stmt);
	}

	currentScope = currentScope->parent;

	return ret;
}
std::unique_ptr<FuncDeclStmt> Parser::ParseFunc() {
	Type* retType = currentScope->FindType(tokenizer.Get());
	assert(retType); tokenizer.Next();
	auto ident = tokenizer.Get();
	assert(ident.type == TokenType::IDENT); tokenizer.Next();
	assert(tokenizer.Next().type == TokenType::OPEN_PARENTH);
	assert(currentScope->FindFunc(ident) == nullptr);

	std::vector<std::unique_ptr<VarDeclStmt>> params;
	while (tokenizer.Get().type != TokenType::CLOSED_PARENTH) {
		auto var = ParseVarDecl(true);
		if (!var) break;

		params.push_back(std::move(var));
		if (tokenizer.Get().type == TokenType::CLOSED_PARENTH) {
			break;
		}
		assert(tokenizer.Next().type == TokenType::COMMA);
	}
	assert(tokenizer.Next().type == TokenType::CLOSED_PARENTH);

	if (tokenizer.Get().type == TokenType::SEMICOLON) {
		tokenizer.Next();
		return std::make_unique<FuncDeclStmt>(retType, ident, &params);
	}
	
	assert(tokenizer.Next().type == TokenType::OPEN_BRACE);
	auto definition = ParseBlock();
	assert(tokenizer.Next().type == TokenType::CLOSED_BRACE);

	std::vector<Variable> vars;
	auto funcScope = currentScope->children.back().get();
	for (auto &param : params) {
		vars.push_back(param->var);
		funcScope->vars.push_back(std::make_unique<Variable>(param->var));
	}
	currentScope->funcs.push_back(std::make_unique<Function>(true, retType, ident, vars));
	return std::unique_ptr<FuncDeclStmt>{new FuncDeclStmt(retType, ident, definition, &params)};
}
std::unique_ptr<ReturnStmt> Parser::ParseReturn() {
	assert(tokenizer.Next().type == TokenType::RETURN);
	auto ret = std::make_unique<ReturnStmt>(std::move(ParseExpr()));
	assert(tokenizer.Next().type == TokenType::SEMICOLON);
	return ret;
}

std::unique_ptr<Statement> Parser::ParseStmt() {
	while (tokenizer.Get().type == TokenType::TYPE_STRUCT) {
		auto t = ParseType();
		if (t) currentScope->types.emplace_back(std::move(t));
	}

	switch(tokenizer.Get().type) {
		case TokenType::IF: return ParseIf();
		case TokenType::WHILE: return ParseWhile();
		case TokenType::OPEN_BRACE: return ParseBlock();
		case TokenType::RETURN: return ParseReturn();
	}

	if (tokenizer.Get().type >= TokenType::TYPES_BEGIN && tokenizer.Get().type <= TokenType::TYPES_END || (currentScope->FindType(tokenizer.Get()))) {
		auto idx = tokenizer.GetIdx();
		Token t;
		while ((t = tokenizer.Next()).type != TokenType::SEMICOLON && t.type != TokenType::NONE) {
			if (t.type == TokenType::IDENT) {
				if ((t = tokenizer.Next()).type == TokenType::OPEN_PARENTH) {
					tokenizer.SetIdx(idx);

					return ParseFunc();
				}
				tokenizer.SetIdx(idx);
				break;
			}
		}
		return ParseVarDecl();
	}
	else if (tokenizer.Get().type == TokenType::IDENT) {
		auto idx = tokenizer.GetIdx();
		tokenizer.Next();
		if (tokenizer.Get().type != TokenType::OPEN_PARENTH) {
			tokenizer.SetIdx(idx);
			return ParseVarAssign();
		}
		tokenizer.Next();

		tokenizer.SetIdx(idx);
		auto funccall = ParsePrimaryExpr();
		assert(tokenizer.Next().type == TokenType::SEMICOLON);

		return std::make_unique<ExpressionStmt>(std::move(funccall));
	}

	return nullptr;
}

const Type *Parser::GetType() {
	bool isMulti = tokenizer.Get().type == TokenType::TYPE_STRUCT || tokenizer.Get().type == TokenType::TYPE_ENUM;
	if (isMulti) {
		tokenizer.Next();
	}

	Token typeName = tokenizer.Next();

	return currentScope->FindType(typeName);
}

void Parser::Parse() {
	std::unique_ptr<Statement> stmt{};
	while ((stmt = std::move(ParseStmt()))) {
		currentScope->block.AddStmt(stmt);
	}
}

Parser::Parser(std::string_view code): tokenizer(code) {
	currentScope = &globalScope;

	// Generates primitives
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_VOID, 0, 0, "void" }, 0, 0, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_BOOL, 0, 0, "bool" }, 1, 1, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_CHAR, 0, 0, "char" }, 1, 1, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_SHORT, 0, 0, "short" }, 2, 2, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_INT, 0, 0, "int" }, 4, 4, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_LONG, 0, 0, "long" }, 8, 8, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_FLOAT, 0, 0, "float" }, 4, 4, std::monostate{}
	));
	globalScope.types.emplace_back(std::make_unique<Type>(
		Token{ TokenType::TYPE_DOUBLE, 0, 0, "double" }, 8, 8, std::monostate{}
	));
}

#include <iostream>

namespace {
	void PrintIdent(std::size_t ident = 0) {
		std::cout << std::string(ident * 2, ' ');
	}
	void PrintExpr(const Expression* expr, std::size_t ident = 0) {
		if (!expr) return;
		switch (expr->type) {
			case ExpressionType::VALUE: {
				PrintIdent(ident);
				std::cout << static_cast<const ValueExpr*>(expr)->val.value;
				break;
			}
			case ExpressionType::BINARY:{
				auto cast = static_cast<const BinaryExpression*>(expr);
				PrintIdent(ident + 1);
				std::cout << "LHS:\n";
				PrintExpr(cast->lhs.get(), ident + 2);
				std::cout << '\n';
				PrintIdent(ident + 1);
				std::cout << "Op: " << cast->op.value << "\n";
				PrintIdent(ident + 1);
				std::cout << "RHS:\n";
				PrintExpr(cast->rhs.get(), ident + 2);
				std::cout << "\n";
				break;
			}
			case ExpressionType::FUNCCALL: {
				auto cast = static_cast<const FuncCallExpr*>(expr);
				PrintIdent(ident);
				std::cout << "FUNCCALL: " << cast->func.value << "(\n";
				for (auto& member : cast->params) {
					PrintExpr(member.get(), ident + 1);
					if (member.get() != cast->params.back().get()) {
						PrintIdent(ident + 1);
						std::cout << ",\n";
					}
					else {
						std::cout << '\n';
					}
				}
				PrintIdent(ident);
				std::cout << ")";
				break;
			}
			case ExpressionType::CAST: {
				const auto *cast = static_cast<const CastExpr *>(expr);
				PrintIdent(ident);
				std::cout << "CAST " << cast->origType->name.value << " -> " << cast->finalType->name.value << ":\n";
				PrintExpr(cast->expr.get(), ident + 1);
			}
		}
	}
	void PrintStatement(const Statement* stmt, std::size_t ident = 0) {
		if (!stmt) return;
		switch (stmt->type) {
			case StatementType::VARDECL: {
				auto cast = static_cast<const VarDeclStmt*>(stmt);
				PrintIdent(ident);
				std::cout << "VarDecl: " << cast->var.name.value << ' ' << cast->var.type->name.value << '\n';
				PrintIdent(ident);
				std::cout << "Value:\n";
				PrintExpr(cast->expr.get(), ident + 1);
				break;
			}
			case StatementType::IF: {
				auto cast = static_cast<const IfStmt*>(stmt);
				PrintIdent(ident);
				std::cout << "IF("; PrintExpr(cast->condition.get()); std::cout << ')\n';
				PrintStatement(cast->then.get(), ident);
				if (cast->els) {
					PrintIdent(ident);
					std::cout << "ELSE:\n";
					PrintStatement(cast->els.get(), ident);
				}
				break;
			}
			case StatementType::FUNCDECL: {
				auto cast = static_cast<const FuncDeclStmt*>(stmt);
				PrintIdent(ident);
				std::cout << "FUNC: " << cast->returnType->name.value << ", " << cast->name.value << '(';
				for (std::size_t i = 0; i < cast->params.size(); ++i) {
					std::cout << cast->params[i]->var.type->name.value << ' ' << cast->params[i]->var.name.value;
					if(i < cast->params.size() - 1){
						std::cout << ", ";
					}
				}
				std::cout << ")\n";
				PrintStatement(cast->definition.get(), ident + 1);
				PrintIdent(ident);
				std::cout << "\n";
				break;
			}
			case StatementType::EXPRSTMT: {
				PrintExpr(static_cast<const ExpressionStmt *>(stmt)->expr.get(), ident);
				break;
			}
			case StatementType::BLOCK: {
				auto cast = static_cast<const BlockStmt*>(stmt);
				for (auto& stmt : cast->stmts) {
					PrintStatement(stmt.get(), ident + 1);
				}
			}
		}
	}
}
void Scope::PrintAST(std::size_t ident) const {
	for (const auto& stmt : block.stmts) {
		PrintStatement(stmt.get(), ident);
	}
	for (const auto& child : children) {
		child->PrintAST(ident + 1);
	}
}
void Parser::PrintAST(std::size_t ident) const {
	globalScope.PrintAST(ident);
}
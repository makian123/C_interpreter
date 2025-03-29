#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include <variant>
#include <utility>
#include <bit>
#include <optional>

#include "tokenizer.hpp"

struct Type;

enum class Modifiers : std::uint8_t {
	CONST = 1 << 1,
	STATIC = 1 << 2,
	INLINE = 1 << 3
};

struct Variable {
	const Type* type;
	Token name;
	Modifiers mods = static_cast<Modifiers>(0);

	void AddMod(Modifiers mod) {
		using MODUNDER = std::underlying_type_t<Modifiers>;
		mods = static_cast<Modifiers>(static_cast<MODUNDER>(mods) | static_cast<MODUNDER>(mod));
	}
	void RemoveMod(Modifiers mod) {
		using MODUNDER = std::underlying_type_t<Modifiers>;
		mods = static_cast<Modifiers>(static_cast<MODUNDER>(mods) & ~static_cast<MODUNDER>(mod));
	}
	bool IsMod(Modifiers mod) const {
		using MODUNDER = std::underlying_type_t<Modifiers>;
		return (static_cast<MODUNDER>(mods) & static_cast<MODUNDER>(mod)) != 0;
	}
};

struct Type {
	struct Structure {
		struct Member {
			Variable var;
			std::size_t offset = 0;
		};

		bool defined;
		std::vector<Member> members;
	};
	struct Array {
		std::size_t sz;
		Type* underlyingType;
	};
	struct Pointer {
		Type* underlyingType;
	};

	Token name;
	std::size_t size;
	std::size_t alignment;

	std::variant<std::monostate, Structure, Array, Pointer> optionalData;

	bool operator==(const Type &other) const {
		return 
			name.value == other.name.value && 
			size == other.size &&
			alignment == other.alignment;
	}
};
struct Typedef {
	Token originalName;
	Token newName;
};

struct Function {
	bool defined = false;
	Type *returnType = nullptr;
	Token name;
	std::vector<Variable> params;

	[[nodiscard]] std::string GenerateSignature() const;
};

enum class ExpressionType: std::uint8_t {
	NONE,
	BINARY,
	VALUE,
	UNARY,
	FUNCCALL,
	CAST
};
struct Expression {
	ExpressionType type;

	Expression(ExpressionType t = ExpressionType::NONE) : type(t) {}
	virtual ~Expression() {};
};
struct ValueExpr: Expression {
	Token val;

	ValueExpr(Token value): val(value), Expression(ExpressionType::VALUE) {}
};
struct BinaryExpression : Expression {
	std::unique_ptr<Expression> lhs, rhs;
	Token op;

	BinaryExpression(std::unique_ptr<Expression> &&left, Token operat, std::unique_ptr<Expression> &&right)
		: lhs(std::move(left)), op(operat), rhs(std::move(right)), Expression(ExpressionType::BINARY) {}
};
struct FuncCallExpr : Expression {
	Token func;
	std::vector<std::unique_ptr<Expression>> params;

	FuncCallExpr(Token name, std::vector<std::unique_ptr<Expression>> &&par) : func(name), params{std::move(par)}, Expression(ExpressionType::FUNCCALL) {}
};
struct CastExpr : Expression {
	const Type *finalType;
	const Type *origType;
	std::unique_ptr<Expression> expr;

	CastExpr(const Type *src, const Type *dest, std::unique_ptr<Expression> &&express): 
		finalType(dest), 
		origType(src), 
		expr(std::move(express)), 
		Expression(ExpressionType::CAST) {}
};
struct UnaryExpr: Expression{
	std::unique_ptr<ValueExpr> expr;
	Token op;

	UnaryExpr(std::unique_ptr<ValueExpr> &&expression, Token oper) : expr(std::move(expression)), op(oper), Expression(ExpressionType::UNARY) {}
};

enum class StatementType: std::uint8_t {
	NONE,
	VARDECL,
	VARASSIGN,
	FUNCDECL,
	BLOCK,
	IF,
	WHILE,
	FOR,
	BREAK,
	CONTINUE,
	EXPRSTMT,
	RETURN,
};
struct Statement {
	StatementType type;

	Statement(StatementType t = StatementType::NONE) : type(t) {}
	virtual ~Statement() {};
};
struct BlockStmt: Statement {
	std::vector<std::unique_ptr<Statement>> stmts;

	BlockStmt() : Statement(StatementType::BLOCK) {}
	void AddStmt(std::unique_ptr<Statement> &stmt) {
		stmts.push_back(std::move(stmt));
	}
};
struct VarDeclStmt : Statement {
	Variable var;
	std::unique_ptr<Expression> expr;

	VarDeclStmt(Token ident, const Type *t, Modifiers mods = static_cast<Modifiers>(0), std::unique_ptr<Expression> &&init = nullptr) : var{t, ident, mods}, expr(std::move(init)), Statement(StatementType::VARDECL) {}
	VarDeclStmt(VarDeclStmt&& other) noexcept : var(std::move(other.var)), expr(std::move(other.expr)), Statement(StatementType::VARDECL) {}
};
struct VarAssignStmt : Statement {
	Token name;
	std::unique_ptr<Expression> val;

	VarAssignStmt(Token ident, std::unique_ptr<Expression> &&expr) : name(ident), val(std::move(expr)), Statement(StatementType::VARASSIGN){}
};
struct IfStmt : Statement {
	std::unique_ptr<Expression> condition;
	std::unique_ptr<Statement> then, els;

	IfStmt(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> ifTrue, std::unique_ptr<Statement> ifFalse = nullptr)
		: condition(std::move(cond)), then(std::move(ifTrue)), els(std::move(ifFalse)), Statement(StatementType::IF) {}
};
struct WhileStmt : Statement {
	std::unique_ptr<Expression> condition;
	std::unique_ptr<Statement> then;

	WhileStmt(std::unique_ptr<Expression> &&cond, std::unique_ptr<Statement> &&ifTrue)
		: condition(std::move(cond)), then(std::move(ifTrue)), Statement(StatementType::WHILE) {}
};
struct ForStmt : Statement {
	std::unique_ptr<Statement> initial;
	std::unique_ptr<Expression> condition;
	std::unique_ptr<Statement> postLoop;

	std::unique_ptr<Statement> then;

	ForStmt(std::unique_ptr<Statement> &&initialize, std::unique_ptr<Expression> &&cond, std::unique_ptr<Statement> &&postLoop, std::unique_ptr<Statement> &&ifTrue)
		: initial(std::move(initialize)), condition(std::move(cond)), postLoop(std::move(postLoop)), then(std::move(ifTrue)), Statement(StatementType::FOR) {}
};
struct BreakStmt : Statement{
	BreakStmt() : Statement(StatementType::BREAK) {}
};
struct ContinueStmt : Statement {
	ContinueStmt() : Statement(StatementType::CONTINUE) {}
};
struct FuncDeclStmt: Statement{
	Type* returnType;
	Token name;
	std::vector<std::unique_ptr<VarDeclStmt>> params;
	std::unique_ptr<BlockStmt> definition;
	std::unique_ptr<Expression> retVal;
	FuncDeclStmt(Type* ret, Token nam, std::unique_ptr<BlockStmt>& defined, std::optional<std::vector<std::unique_ptr<VarDeclStmt>>*> pars = std::nullopt)
		: returnType(ret), name(nam), definition(std::move(defined)), Statement(StatementType::FUNCDECL) {
		if (!pars) return;

		for (auto& otherParam: *pars.value()) {
			params.push_back(std::move(otherParam));
		}
	}
	FuncDeclStmt(Type* ret, Token nam, std::optional<std::vector<std::unique_ptr<VarDeclStmt>>*> pars = std::nullopt)
		: returnType(ret), name(nam), definition(nullptr), Statement(StatementType::FUNCDECL) {
		if (!pars) return;

		for (auto& otherParam : *pars.value()) {
			params.push_back(std::move(otherParam));
		}
	}
};
struct ReturnStmt : Statement {
	std::unique_ptr<Expression> ret;
	ReturnStmt(std::unique_ptr<Expression> &&exp) : ret{ std::move(exp) }, Statement(StatementType::RETURN) {}
};
struct ExpressionStmt : Statement {
	std::unique_ptr<Expression> expr;
	ExpressionStmt(std::unique_ptr<Expression> &&exp) : expr{ std::move(exp) }, Statement(StatementType::EXPRSTMT) {}
};

struct Scope {
	Scope* parent = nullptr;

	std::vector<Typedef> typedefs;
	std::vector<std::unique_ptr<Type>> types;
	std::vector<std::unique_ptr<Scope>> children;
	std::vector<std::unique_ptr<Variable>> vars;
	std::vector<std::unique_ptr<Function>> funcs;
	BlockStmt block;

	Type* FindType(Token name);
	Variable *FindVar(Token name);
	Function *FindFunc(Token name);

	const Type *FindType(Token name) const;
	const Variable *FindVar(Token name, bool thisScope = false) const;
	const Function *FindFunc(Token name) const;

	void PrintAST(std::size_t ident = 0) const;
};

class Parser {
	Tokenizer tokenizer;
	Scope globalScope;
	Scope* currentScope;


	std::unique_ptr<Type> ParseType();
	std::unique_ptr<Expression> ParsePrimaryExpr();
	std::unique_ptr<Expression> ParseExpr(int precedence = 0);

	std::unique_ptr<VarDeclStmt> ParseVarDecl(bool param = false);
	std::unique_ptr<VarAssignStmt> ParseVarAssign(bool checkSemicolon = true);
	std::unique_ptr<IfStmt> ParseIf();
	std::unique_ptr<WhileStmt> ParseWhile();
	std::unique_ptr<ForStmt> ParseFor();
	std::unique_ptr<BlockStmt> ParseBlock();
	std::unique_ptr<FuncDeclStmt> ParseFunc();
	std::unique_ptr<ReturnStmt> ParseReturn();
	std::unique_ptr<Statement> ParseStmt(bool checkSemicolon = true);

	const Type *GetType();
public:
	Parser(std::string_view code);

	void Parse();
	void PrintAST(std::size_t off = 0) const;

	const Type *EvalType(Expression &expr, Scope *scope = nullptr) const;

	const Scope &GetGlobalScope() const { return globalScope; }
	Scope &GetGlobalScope() { return globalScope; }
};
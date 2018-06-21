#include "lexer.hpp"
#include "ast.hpp"
#include <map>

static TokenResult CurTok;
TokenResult getNextToken() {
	return CurTok = gettok();
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "LogError: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = std::make_unique<NumberExprAST>(CurTok.numVal);
	getNextToken(); // consume the number
	return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken(); // eat (.
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok.thisChar != ')')
		return LogError("expected ')'");
	getNextToken(); // eat ).
	return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = CurTok.identifierStr;

	getNextToken();  // eat identifier.

	if (CurTok.thisChar != '(') // Simple variable ref.
		return std::make_unique<VariableExprAST>(IdName);

	// Call.
	getNextToken();  // eat (
	std::vector<std::unique_ptr<ExprAST>> Args;
	if (CurTok.thisChar != ')') {
		while (1) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok.thisChar == ')')
				break;

			if (CurTok.thisChar != ',')
				return LogError("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	// Eat the ')'.
	getNextToken();

	return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
std::unique_ptr<ExprAST> ParsePrimary() {
	switch (CurTok.token) {
	default:
		return LogError("unknown token when expecting an expression");
	case tok_identifier:
		return ParseIdentifierExpr();
	case tok_number:
		return ParseNumberExpr();
	case tok_none:
		if (CurTok.thisChar == '(')
			return ParseParenExpr();
		return LogError("unknown token when expecting an expression");
	}
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence() {
	if (!isascii(CurTok.thisChar))
		return -1;

	// Make sure it's a declared binop.
	int TokPrec = BinopPrecedence[CurTok.thisChar];
	if (TokPrec <= 0) return -1;
	return TokPrec;
}
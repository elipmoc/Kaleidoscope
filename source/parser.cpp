#include "parser.hpp"
#include "ast.hpp"
#include "llvm/IR/Verifier.h"

TokenResult Parser::getNextToken() {
	return curTok = gettok();
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> Parser::LogError(const char *Str) {
	fprintf(stderr, "LogError: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> Parser::LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
	auto Result = std::make_unique<NumberExprAST>(curTok.numVal);
	getNextToken(); // consume the number
	return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
	getNextToken(); // eat (.
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (curTok.thisChar != ')')
		return LogError("expected ')'");
	getNextToken(); // eat ).
	return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
	std::string IdName = curTok.identifierStr;

	getNextToken();  // eat identifier.

	if (curTok.thisChar != '(') // Simple variable ref.
		return std::make_unique<VariableExprAST>(IdName);

	// Call.
	getNextToken();  // eat (
	std::vector<std::unique_ptr<ExprAST> > Args;
	if (curTok.thisChar != ')') {
		while (1) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (curTok.thisChar == ')')
				break;

			if (curTok.thisChar != ',')
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
std::unique_ptr<ExprAST> Parser::ParsePrimary() {
	switch (curTok.token) {
	default:
		return LogError("unknown token when expecting an expression");
	case tok_identifier:
		return ParseIdentifierExpr();
	case tok_number:
		return ParseNumberExpr();
	case tok_none:
		if (curTok.thisChar == '(')
			return ParseParenExpr();
		return LogError("unknown token when expecting an expression");
	}
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int Parser::GetTokPrecedence() {
	if (!isascii(curTok.thisChar))
		return -1;

	// Make sure it's a declared binop.
	int TokPrec = binopPrecedence[curTok.thisChar];
	if (TokPrec <= 0) return -1;
	return TokPrec;
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> Parser::ParseExpression() {
	auto LHS = ParsePrimary();
	if (!LHS)
		return nullptr;

	return ParseBinOpRHS(0, std::move(LHS));
}

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec,std::unique_ptr<ExprAST> LHS) {
	// If this is a binop, find its precedence.
	while (1) {
		int TokPrec = GetTokPrecedence();

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (TokPrec < ExprPrec)
			return LHS;
		// Okay, we know this is a binop.
		int BinOp = curTok.thisChar;
		getNextToken();  // eat binop

						 // Parse the primary expression after the binary operator.
		auto RHS = ParsePrimary();
		if (!RHS)
			return nullptr;
		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		// Merge LHS/RHS.
		LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS),std::move(RHS));
	}  // loop around to the top of the while loop.
}

/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> Parser::ParsePrototype() {
	if (curTok.token != tok_identifier)
		return LogErrorP("Expected function name in prototype");

	std::string FnName = curTok.identifierStr;
	getNextToken();

	if (curTok.thisChar != '(')
		return LogErrorP("Expected '(' in prototype");

	// Read the list of argument names.
	std::vector<std::string> ArgNames;
	while (getNextToken().token == tok_identifier)
		ArgNames.push_back(curTok.identifierStr);
	if (curTok.thisChar != ')')
		return LogErrorP("Expected ')' in prototype");

	// success.
	getNextToken();  // eat ')'.

	return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> Parser::ParseDefinition() {
	getNextToken();  // eat def.
	auto Proto = ParsePrototype();
	if (!Proto) return nullptr;

	if (auto E = ParseExpression())
		return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> Parser::ParseExtern() {
	getNextToken();  // eat extern.
	return ParsePrototype();
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr() {
	if (auto E = ParseExpression()) {
		// Make an anonymous proto.
		auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
		return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}


void Parser::HandleDefinition() {
	if (auto FnAST=ParseDefinition()) {
		if (auto* FnIR = FnAST->codegen(*codeGen)) {
			fprintf(stderr, "Parsed a function definition.\n");
			FnIR->print(llvm::errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

void Parser::HandleExtern() {
	if (auto ProtoAST = ParseExtern()) {
		if (auto* FnIR = ProtoAST->codegen(*codeGen)) {
			fprintf(stderr, "Parsed an extern\n");
			FnIR->print(llvm::errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

void Parser::HandleTopLevelExpression() {
	// Evaluate a top-level expression into an anonymous function.
	if (auto FnAST = ParseTopLevelExpr()) {
		if (auto* FnIR = FnAST->codegen(*codeGen)) {
			fprintf(stderr, "Parsed a top-level expr\n");
			FnIR->print(llvm::errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

/// top ::= definition | external | expression | ';'

void Parser::MainLoop() {
	while (1) {
		fprintf(stderr, "ready> ");
		switch (curTok.token) {
		case tok_eof:
			return;
		case tok_def:
			HandleDefinition();
			break;
		case tok_extern:
			HandleExtern();
			break;
		case tok_none:
			if (curTok.thisChar == ';') {
				getNextToken();
				break;
			}
		default:
			HandleTopLevelExpression();
			break;
		}
	}
}

void Parser::Do() {
	fprintf(stderr, "ready> ");
	getNextToken();
	MainLoop();
	codeGen->theModule->print(llvm::errs(), nullptr);
}
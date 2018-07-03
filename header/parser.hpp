#pragma once

#include "lexer.hpp"
#include <memory>
#include <map>
#include "codeGen.hpp"

class ExprAST;
class PrototypeAST;
class FunctionAST;


class Parser {
	TokenResult curTok;

	/// BinopPrecedence - This holds the precedence for each binary operator that is
	/// defined.
	std::map<char, int> binopPrecedence;
	std::unique_ptr<CodeGen> codeGen;

public:

	Parser(std::unique_ptr<CodeGen>&& codeGen):codeGen(std::move(codeGen)) {
		// Install standard binary operators.
		// 1 is lowest precedence.
		binopPrecedence['<'] = 10;
		binopPrecedence['+'] = 20;
		binopPrecedence['-'] = 20;
		binopPrecedence['*'] = 40;  // highest.
		
	};

private:

	TokenResult getNextToken();

	/// numberexpr ::= number
	std::unique_ptr<ExprAST> ParseNumberExpr();
	/// parenexpr ::= '(' expression ')'
	std::unique_ptr<ExprAST> ParseParenExpr();

	/// identifierexpr
	///   ::= identifier
	///   ::= identifier '(' expression* ')'
	std::unique_ptr<ExprAST> ParseIdentifierExpr();

	/// primary
	///   ::= identifierexpr
	///   ::= numberexpr
	///   ::= parenexpr
	std::unique_ptr<ExprAST> ParsePrimary();

	/// GetTokPrecedence - Get the precedence of the pending binary operator token.
	int GetTokPrecedence();

	/// expression
	///   ::= primary binoprhs
	///
	std::unique_ptr<ExprAST> ParseExpression();

	/// binoprhs
	///   ::= ('+' primary)*
	std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);

	/// prototype
	///   ::= id '(' id* ')'
	std::unique_ptr<PrototypeAST> ParsePrototype();

	/// definition ::= 'def' prototype expression
	std::unique_ptr<FunctionAST> ParseDefinition();

	/// external ::= 'extern' prototype
	std::unique_ptr<PrototypeAST> ParseExtern();

	/// toplevelexpr ::= expression
	std::unique_ptr<FunctionAST> ParseTopLevelExpr();

	/// top ::= definition | external | expression | ';'
public : 
	void MainLoop();
	void Do();
private:

	void HandleDefinition();

	void HandleExtern();

	void HandleTopLevelExpression();

};
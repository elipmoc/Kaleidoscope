#pragma once

#include<string>
enum Token {
	tok_eof=-1,
	// commands
	tok_def=-2,
	tok_extern = -3,

	// primary
	tok_identifier = -4,
	tok_number = -5,

	//no match
	tok_none=-6,

	tok_if=-7,
	tok_then=-8,
	tok_else=-9,
	tok_for=-10,
	tok_in=-11
};

struct TokenResult {
	Token token;
	std::string identifierStr;
	double numVal=0;
	int thisChar=EOF;
};

/// gettok - Return the next token from standard input.
TokenResult gettok();
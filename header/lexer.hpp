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
	tok_none=-6
};

struct TokenResult {
	Token token;
	std::string identifierStr;
	double numVal=0;
	int thisChar=EOF;
};

/// gettok - Return the next token from standard input.
TokenResult gettok();
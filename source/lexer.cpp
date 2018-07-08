#include "lexer.hpp"

/// gettok - Return the next token from standard input.
TokenResult gettok() {
	static int LastChar = ' ';

	// Skip any whitespace.
	while (isspace(LastChar))
		LastChar = getchar();
	TokenResult tr;
	if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
		tr.identifierStr = LastChar;
		while (isalnum((LastChar = getchar())))
			tr.identifierStr += LastChar;

		if (tr.identifierStr == "def") 
			tr.token = tok_def;
		else if (tr.identifierStr == "extern")
			tr.token = tok_extern;
		else if (tr.identifierStr == "if")
			tr.token=tok_if;
		else if (tr.identifierStr == "then")
			tr.token=tok_then;
		else if (tr.identifierStr == "else")
			tr.token=tok_else;
		else if (tr.identifierStr == "for")
			tr.token = tok_for;
		else if (tr.identifierStr == "in")
			tr.token = tok_in;
		else tr.token = tok_identifier;
		return tr;
	}
	if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		tr.numVal = strtod(NumStr.c_str(), NULL);
		tr.token = tok_number;
		return tr;
	}
	if (LastChar == '#') {
		// Comment until end of line.
		do
			LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if (LastChar != EOF)
			return gettok();
	}
	// Check for end of file.  Don't eat the EOF.
	if (LastChar == EOF) {
		tr.token = tok_eof;
		return tr;
	}

	// Otherwise, just return the character as its ascii value.
	tr.thisChar = LastChar;
	tr.token = tok_none;
	LastChar = getchar();
	return tr;
}
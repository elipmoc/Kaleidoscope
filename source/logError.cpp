#include "logError.hpp"
#include "ast.hpp"

namespace LogError {

	void LogErrorBase(const char * Str)
	{
			fprintf(stderr, "LogError: %s\n", Str);
	}

	llvm::Value * LogErrorV(const char * Str)
	{
		LogErrorBase(Str);
		return nullptr;
	}

	std::unique_ptr<ExprAST> LogError(const char * Str)
	{
		LogErrorBase(Str);
		return std::unique_ptr<ExprAST>();
	}

	std::unique_ptr<PrototypeAST> LogErrorP(const char * Str)
	{
		LogErrorBase(Str);
		return std::unique_ptr<PrototypeAST>();
	}
}
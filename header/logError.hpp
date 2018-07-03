#pragma once
#include "llvm/IR/IRBuilder.h"

class ExprAST;
class PrototypeAST;

namespace LogError {
	void LogErrorBase(const char *Str); 
	llvm::Value* LogErrorV(const char *Str);
	std::unique_ptr<ExprAST> LogError(const char *Str);
	std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
}
#pragma once
#include "llvm\IR\IRBuilder.h"
#include "llvm\IR\Value.h"

class CodeGen {
public:
	llvm::LLVMContext theContext;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> theModule;
	std::map<std::string, llvm::Value *> namedValues;
	CodeGen():builder(theContext) {
		theModule= std::make_unique<llvm::Module>("my cool jit", theContext);
	}

	llvm::Value *LogErrorV(const char *Str) {
		fprintf(stderr, "LogError: %s\n", Str);
		return nullptr;
	}
};
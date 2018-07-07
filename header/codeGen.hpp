#pragma once
#include "llvm\IR\IRBuilder.h"
#include "llvm\IR\Value.h"
#include "llvm\IR\LegacyPassManager.h"
#include "KaleidoscopeJIT.hpp"
#include "llvm\Transforms\InstCombine\InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm\Support\TargetSelect.h"
#include "KaleidoscopeJIT.hpp"
#include "ast.hpp"


class CodeGen {
public:
	llvm::LLVMContext theContext;
	llvm::IRBuilder<> builder;
	std::unique_ptr<llvm::Module> theModule;
	std::unique_ptr<llvm::legacy::FunctionPassManager> theFPM;
	std::unique_ptr<llvm::orc::KaleidoscopeJIT> theJIT;
	std::map<std::string, llvm::Value *> namedValues;
	std::map<std::string, std::unique_ptr<PrototypeAST>> functionProtos;
	CodeGen():builder(theContext) {
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();
		theJIT = std::make_unique<llvm::orc::KaleidoscopeJIT>();
		theModule = std::make_unique<llvm::Module>("my cool jit", theContext);
	}

	void InitializeModuleAndPassManager(void) {
		// Open a new module.
		theModule = std::make_unique<llvm::Module>("my cool jit", theContext);
		theModule->setDataLayout(theJIT->getTargetMachine().createDataLayout());

		// Create a new pass manager attached to it.
		theFPM = std::make_unique<llvm::legacy::FunctionPassManager>(theModule.get());

		// Do simple "peephole" optimizations and bit-twiddling optzns.
		theFPM->add(llvm::createInstructionCombiningPass());
		// Reassociate expressions.
		theFPM->add(llvm::createReassociatePass());
		// Eliminate Common SubExpressions.
		theFPM->add(llvm::createGVNPass());
		// Simplify the control flow graph (deleting unreachable blocks, etc).
		theFPM->add(llvm::createCFGSimplificationPass());

		theFPM->doInitialization();
	}

	auto addModuleToJit() {
		return theJIT->addModule(std::move(theModule));
	}

	void removeModuleFromJit(const decltype(theJIT->addModule(std::move(theModule)))& h) {
		theJIT->removeModule(h);
	}
};
#include "ast.hpp"
#include "codeGen.hpp"
#include "logError.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

llvm::Value *NumberExprAST::codegen(CodeGen& codeGen) {
	return llvm::ConstantFP::get(codeGen.theContext, llvm::APFloat(Val));
}

llvm::Value * VariableExprAST::codegen(CodeGen& codeGen)
{
	// Look this variable up in the function.
	llvm::Value* V = codeGen.namedValues[Name];
	if (!V)
		LogError::LogErrorV("Unknown variable name");
	return V;
}

llvm::Value *BinaryExprAST::codegen(CodeGen& codeGen) {
	llvm::Value *L = LHS->codegen(codeGen);
	llvm::Value *R = RHS->codegen(codeGen);
	if (!L || !R)
		return nullptr;
	switch (Op) {
	case '+':
		return codeGen.builder.CreateFAdd(L, R, "addtmp");
	case '-':
		return codeGen.builder.CreateFSub(L, R, "subtmp");
	case '*':
		return codeGen.builder.CreateFMul(L, R, "multmp");
	case '<':
		L = codeGen.builder.CreateFCmpULT(L, R, "cmptmp");
		// Convert bool 0/1 to double 0.0 or 1.0
		return codeGen.builder.CreateUIToFP(L, llvm::Type::getDoubleTy(codeGen.theContext),
			"booltmp");
	default:
		return LogError::LogErrorV("invalid binary operator");
	}
	llvm::llvm_unreachable_internal("", "", 0);

}

llvm::Value *CallExprAST::codegen(CodeGen& codeGen) {
	// Look up the name in the global module table.
	llvm::Function *CalleeF = codeGen.theModule->getFunction(Callee);
	if (!CalleeF)
		return LogError::LogErrorV("Unknown function referenced");

	// If argument mismatch error.
	if (CalleeF->arg_size() != Args.size())
		return LogError::LogErrorV("Incorrect # arguments passed");

	std::vector<llvm::Value *> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
		ArgsV.push_back(Args[i]->codegen(codeGen));
		if (!ArgsV.back())
			return nullptr;
	}

	return codeGen.builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function *PrototypeAST::codegen(CodeGen& codeGen) {
	// Make the function type:  double(double,double) etc.
	std::vector<llvm::Type*> Doubles(Args.size(),
		llvm::Type::getDoubleTy(codeGen.theContext));
	llvm::FunctionType *FT =
		llvm::FunctionType::get(llvm::Type::getDoubleTy(codeGen.theContext), Doubles, false);

	llvm::Function *F =
		llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, codeGen.theModule.get());
	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto &Arg : F->args())
		Arg.setName(Args[Idx++]);

	return F;
}

llvm::Function *FunctionAST::codegen(CodeGen& codeGen) {
	// First, check for an existing function from a previous 'extern' declaration.
	llvm::Function *TheFunction = codeGen.theModule->getFunction(Proto->getName());

	if (!TheFunction)
		TheFunction = Proto->codegen(codeGen);

	if (!TheFunction)
		return nullptr;

	if (!TheFunction->empty())
		return (llvm::Function*)LogError::LogErrorV("Function cannot be redefined.");
	// Create a new basic block to start insertion into.
	llvm::BasicBlock *BB = llvm::BasicBlock::Create(codeGen.theContext, "entry", TheFunction);
	codeGen.builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	codeGen.namedValues.clear();
	for (auto &Arg : TheFunction->args())
		codeGen.namedValues[Arg.getName()] = &Arg;
	if (llvm::Value *RetVal = Body->codegen(codeGen)) {
		// Finish off the function.
		codeGen.builder.CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		llvm::verifyFunction(*TheFunction);

		return TheFunction;
	}
	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}
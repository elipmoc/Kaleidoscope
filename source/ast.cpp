#include "ast.hpp"
#include "codeGen.hpp"
#include "logError.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"

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
	llvm::Function *CalleeF = getFunction(Callee,codeGen);
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
	// Transfer ownership of the prototype to the FunctionProtos map, but keep a
	// reference to it for use below.
	auto &P = *Proto;
	codeGen.functionProtos[Proto->getName()] = std::move(Proto);
	llvm::Function* TheFunction = getFunction(P.getName(),codeGen);
	if (!TheFunction)
		return nullptr;
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
		codeGen.theFPM->run(*TheFunction);
		return TheFunction;
	}
	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}

llvm::Function * getFunction(std::string Name, CodeGen & codeGen)
{
	// First, see if the function has already been added to the current module.
	if (auto *F = codeGen.theModule->getFunction(Name))
		return F;

	// If not, check whether we can codegen the declaration from some existing
	// prototype.
	auto FI = codeGen.functionProtos.find(Name);
	if (FI != codeGen.functionProtos.end())
		return FI->second->codegen(codeGen);

	// If no existing prototype exists, return null.
	return nullptr;
}

llvm::Value * IfExprAST::codegen(CodeGen & codeGen)
{
	llvm::Value *CondV = Cond->codegen(codeGen);
	if (!CondV)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = codeGen.builder.CreateFCmpONE(
		CondV, llvm::ConstantFP::get(codeGen.theContext, llvm::APFloat(0.0)), "ifcond");
	llvm::Function *TheFunction = codeGen.builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the
	// end of the function.
	llvm::BasicBlock *ThenBB =
		llvm::BasicBlock::Create(codeGen.theContext, "then", TheFunction);
	llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(codeGen.theContext, "else");
	llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(codeGen.theContext, "ifcont");

	codeGen.builder.CreateCondBr(CondV, ThenBB, ElseBB);
	// Emit then value.
	codeGen.builder.SetInsertPoint(ThenBB);

	llvm::Value *ThenV = Then->codegen(codeGen);
	if (!ThenV)
		return nullptr;

	codeGen.builder.CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = codeGen.builder.GetInsertBlock();
	// Emit else block.
	TheFunction->getBasicBlockList().push_back(ElseBB);
	codeGen.builder.SetInsertPoint(ElseBB);

	llvm::Value *ElseV = Else->codegen(codeGen);
	if (!ElseV)
		return nullptr;

	codeGen.builder.CreateBr(MergeBB);
	// codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = codeGen.builder.GetInsertBlock();
	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	codeGen.builder.SetInsertPoint(MergeBB);
	llvm::PHINode *PN =
		codeGen.builder.CreatePHI(llvm::Type::getDoubleTy(codeGen.theContext), 2, "iftmp");

	PN->addIncoming(ThenV, ThenBB);
	PN->addIncoming(ElseV, ElseBB);
	return PN;
}

llvm::Value * ForExprAST::codegen(CodeGen & codeGen)
{
	// Emit the start code first, without 'variable' in scope.
	llvm::Value *StartVal = Start->codegen(codeGen);
	if (!StartVal)
		return nullptr;

	// Make the new basic block for the loop header, inserting after current
	// block.
	llvm::Function *TheFunction = codeGen.builder.GetInsertBlock()->getParent();
	llvm::BasicBlock *PreheaderBB = codeGen.builder.GetInsertBlock();
	llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(codeGen.theContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB.
	codeGen.builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
	codeGen.builder.SetInsertPoint(LoopBB);

	// Start the PHI node with an entry for Start.
	llvm::PHINode *Variable =
		codeGen.builder.CreatePHI(llvm::Type::getDoubleTy(codeGen.theContext), 2, VarName);
	Variable->addIncoming(StartVal, PreheaderBB);

	// Within the loop, the variable is defined equal to the PHI node.  If it
	// shadows an existing variable, we have to restore it, so save it now.
	llvm::Value *OldVal = codeGen.namedValues[VarName];
	codeGen.namedValues[VarName] = Variable;

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	if (!Body->codegen(codeGen))
		return nullptr;

	// Emit the step value.
	llvm::Value *StepVal = nullptr;
	if (Step) {
		StepVal = Step->codegen(codeGen);
		if (!StepVal)
			return nullptr;
	}
	else {
		// If not specified, use 1.0.
		StepVal = llvm::ConstantFP::get(codeGen.theContext, llvm::APFloat(1.0));
	}

	llvm::Value *NextVar = codeGen.builder.CreateFAdd(Variable, StepVal, "nextvar");

	// Compute the end condition.
	llvm::Value *EndCond = End->codegen(codeGen);
	if (!EndCond)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = codeGen.builder.CreateFCmpONE(
		EndCond, llvm:: ConstantFP::get(codeGen.theContext, llvm::APFloat(0.0)), "loopcond");

	// Create the "after loop" block and insert it.
	llvm::BasicBlock *LoopEndBB = codeGen.builder.GetInsertBlock();
	llvm::BasicBlock *AfterBB =
		llvm::BasicBlock::Create(codeGen.theContext, "afterloop", TheFunction);

	// Insert the conditional branch into the end of LoopEndBB.
	codeGen.builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// Any new code will be inserted in AfterBB.
	codeGen.builder.SetInsertPoint(AfterBB);

	// Add a new entry to the PHI node for the backedge.
	Variable->addIncoming(NextVar, LoopEndBB);

	// Restore the unshadowed variable.
	if (OldVal)
		codeGen.namedValues[VarName] = OldVal;
	else
		codeGen.namedValues.erase(VarName);

	// for expr always returns 0.0.
	return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(codeGen.theContext));
}

#include <iostream>
#include "llvm/IR/IRBuilder.h"
#include "parser.hpp"
void main()
{
    std::cout << "hello" << std::endl;
	Parser parser;
	parser.MainLoop();
}
#include <iostream>
#include "parser.hpp"
#include "externFunc.hpp"
void main()
{

    std::cout << "hello" << std::endl;
	auto codeGen = std::make_unique<CodeGen>();
	Parser parser(std::move(codeGen));
	parser.Do();
}
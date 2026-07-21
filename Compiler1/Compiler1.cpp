// Compiler1.cpp : Defines the entry point for the application.
//

#include "Compiler1.h"
#include "tokenization.h"
#include "Generator.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "SemanticAnalyzer.h"



int main(int argc, char* argv[])
{

	if (argc < 2)
	{
		//std::cout << "Usage: " << argv[0] << " <filename.at>" << std::endl;
	//	return 1;
	}

	std::fstream input("/mnt/c/Users/james/source/repos/Compiler1/test.cpp", std::ios::in);
	std::stringstream buffer;
	buffer << input.rdbuf();


	std::string content = buffer.str();

    Tokenizer tokenizer(content);

	std::vector<Token> tokens = tokenizer.tokenize(content);

	Parser parser(tokens);

	auto exitNode = parser.ParseProgram();

	SemanticAnalyzer analyzer(exitNode.value());


	analyzer.Analyze();
	Generator gen(std::move(exitNode.value()));


	auto assembly = gen.GenerateProgram();

    std::cout << assembly << std::endl;

	std::fstream out("/mnt/c/Users/james/source/repos/Compiler1/out.asm", std::ios::out);
	out << assembly;

	out.close();
	input.close();

	return 0;
}

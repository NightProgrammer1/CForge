
#include "CForge.h"



std::string executeCommand(const std::string& cmd, int& exitCode) {
    std::string output = "";
    std::array<char, 128> buffer;

    std::string fullCmd = cmd + " 2>&1";

    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        exitCode = -1;
        return "Error: Failed to create pipe.";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    exitCode = WEXITSTATUS(status);

    return output;
}
int Compile(int argc, char* argv[], NodeProg& exitNode, std::string_view assembly)
{
    bool print_assembly = false;
    std::string input_file = "";
    std::string output_file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--print-assembly") {
            print_assembly = true;
        }
        else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        }
        else if (input_file.empty() && arg[0] != '-') {
            input_file = arg;
        }
    }

    if (print_assembly) {
        std::cout << assembly << std::endl;
    }

    std::string outputPath;
    if (!output_file.empty()) {
        outputPath = output_file;
        if (outputPath.find('.') == std::string::npos) {
            outputPath += ".asm";
        }
    }
    else if (!input_file.empty()) {
        size_t lastdot = input_file.find_last_of('.');
        outputPath = (lastdot == std::string::npos) ? input_file + ".asm" : input_file.substr(0, lastdot) + ".asm";
    }
    else {
        outputPath = "output.asm";
    }

    {
        std::ofstream out(outputPath);
        if (!out.is_open()) {
            return -1;
        }
        out << assembly;
    }

    size_t out_lastdot = outputPath.find_last_of('.');
    std::string objPath = (out_lastdot == std::string::npos) ? outputPath + ".o" : outputPath.substr(0, out_lastdot) + ".o";

    std::string nasmCommand = "nasm -f " + std::string(NASM_FORMAT) + " " + outputPath + " -o " + objPath;
    int nasmResult = 0;
    std::string nasmOutput = executeCommand(nasmCommand, nasmResult);

    if (nasmResult != 0) {
        std::remove(outputPath.c_str());
        return nasmResult;
    }

    size_t obj_lastdot = objPath.find_last_of('.');
    std::string exePath = (obj_lastdot == std::string::npos) ? objPath + "out" : objPath.substr(0, obj_lastdot) + ".out";

    std::string linkCommand = "ld " + objPath + " -o " + exePath;
    int linkResult = 0;
    std::string linkOutput = executeCommand(linkCommand, linkResult);

    if (linkResult != 0) {
        // Clean up intermediate files if linking fails
        std::remove(outputPath.c_str());
        std::remove(objPath.c_str());
        return linkResult;
    }

   
    std::remove(outputPath.c_str()); // Deletes the .asm file
    std::remove(objPath.c_str());    // Deletes the .o object file

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: cf" << " <source_file> [-o output_file] [--print-ast] [--print-assembly]\n";
        exit(EXIT_FAILURE);
    }

    std::string inputPath = argv[1];

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        std::cerr << "Error: Could not open input file: " << inputPath << "\n";
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    input.close();

    std::string content = buffer.str();
    std::vector<Token> tokens = Tokenizer::tokenize(content);

    std::pmr::monotonic_buffer_resource arena((1024 * 1024) * 4);
    std::pmr::polymorphic_allocator<NodeExpr> alloc(&arena);
    std::pmr::polymorphic_allocator<Instruction> allocTAC(&arena);

    Parser parser(tokens, alloc);
    auto exitNode = parser.ParseProgram();
    if (!exitNode.has_value()) {
        std::cerr << "Error: Parsing failed.\n";
        exit(EXIT_FAILURE);
    }

    // --- SEMANTIC ANALYSIS ---
    SemanticAnalyzer analyzer(exitNode.value(), alloc);
    analyzer.Analyze();


    // --- CHECK FOR --print-ast (Raw, Unoptimized AST) ---
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--print-ast") {
            PrintAST(exitNode.value());
            std::cout << std::endl;
            return 0; // Exit early if only printing AST
        }
    }
    // --- CONSTANT FOLDER / OPTIMIZATIONS ---
   // auto folder = ConstantFolder(exitNode.value(), alloc);
    //folder.FoldProgram();

    // --- IR LOWERING & CODEGEN ---
    IRLowering lowerer(exitNode.value(), alloc);
    auto ir = lowerer.Lower();

    TACLowering tac(ir, allocTAC);
    auto t = tac.Lower();

    BasicBlockBuilder builder(t, arena);
    auto cfg = builder.Create();

    PreMachineIR machine(cfg);
    auto machIns = machine.Lower();

    LivenessAnalysis analysis;
    for (auto& function : machIns.functions)
    {
        RegisterAllocator reg;
        auto liveInt = analysis.GetLiveIntervals(function);
        auto data = reg.AllocateReg(liveInt, function);

        function.stack_size = data.first;
        function.usedCalleeSaved = data.second;
    }

    PostMachineIR postMachIns(machIns);
    postMachIns.Lower();

    X86Generator gen(machIns);
    auto assembly = gen.GenerateProgram();


    return Compile(argc, argv, exitNode.value(), assembly);
}
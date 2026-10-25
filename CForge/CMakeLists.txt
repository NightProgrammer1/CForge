cmake_minimum_required(VERSION 3.31.6)
project(CForge)

# Add source to this project's executable
add_executable(cf 
    "CForge.cpp" 
    "CForge.h" 
    "tokenization.h" 
    "parser.h" 
    "types.h" 
    "SymbolTable.h" 
    "SemanticAnalyzer.h" 
    "IRLowering.h" 
    "IRTypes.h" 
    "Generator.h" 
    "ThreeAdressCode.h"  
    "BasicBlockBuilder.h" 
    "LivenessAnalysis.h" 
    "PreMachineInstruction.h" 
    "RegisterAllocation.h" 
    "PostMachineInstruction.h"
    "ConstantFolder.h"
    "ASTEvaluator.h"
)

# Set C++20 standard
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET cf PROPERTY CXX_STANDARD 20)
endif()

if(EMSCRIPTEN)
    set_target_properties(cf PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CForge"
    )

    target_link_options(cf PRIVATE
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['FS','callMain','ccall','cwrap']"
    )
else()
    set_target_properties(cf PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CForge"
    )
endif()
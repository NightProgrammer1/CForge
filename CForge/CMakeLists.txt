# CMakeList.txt : CMake project for CForge, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.31.6)
# Add source to this project's executable.
add_executable (cf "CForge.cpp" "CForge.h" "tokenization.h" "parser.h" "types.h" "SymbolTable.h" "SemanticAnalyzer.h" "IRLowering.h" "IRTypes.h" "Generator.h" "ThreeAdressCode.h"  "BasicBlockBuilder.h" "LivenessAnalysis.h" "PreMachineInstruction.h" "RegisterAllocation.h" "PostMachineInstruction.h" )

set_target_properties(cf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CForge"
)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET cf PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

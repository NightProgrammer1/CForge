# CMakeList.txt : CMake project for Compiler1, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.31.6)
# Add source to this project's executable.
add_executable (Compiler1 "Compiler1.cpp" "Compiler1.h" "tokenization.h" "parser.h" "Generator.h" "types.h" "SymbolTable.h" "SemanticAnalyzer.h" "Constant Folder.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Compiler1 PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

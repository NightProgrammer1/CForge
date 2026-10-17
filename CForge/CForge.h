// Compiler1.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include "tokenization.h"
#include "Generator.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "SemanticAnalyzer.h"
#include "IRLowering.h"
#include "ThreeAdressCode.h"
#include "BasicBlockBuilder.h"
#include "LivenessAnalysis.h"
#include "PreMachineInstruction.h"
#include "PostMachineInstruction.h"
//#include "Constant Folder.h"
#include "debug.h"
#pragma once
#include "IRTypes.h"
#include <memory_resource>




class BasicBlockBuilder
{
	
public:
	 BasicBlockBuilder(const TACProgram& program, std::pmr::monotonic_buffer_resource& arena) : alloc(&arena), instructions(program) {

	}
	CFGProgram Create()
	{
		CFGProgram program;
		for (const auto& tac : instructions.functions)
		{
			
			for (const auto& ins : tac.instructions)
			{
				V(ins);
			}
			FinishBlock();
			graph.name = tac.name;
			graph.parameters = tac.parameters;
			graph.stackSize = tac.stackSize;
			program.functions.push_back(std::move(graph));
		}


		/*
		for (auto& block : blocks)
		{
			if (block.ins.empty()) { continue; };
			auto* last = block.ins.back();
			if (auto mov = std::get_if<IRStatementJump>(&last->expr))
			{

			}
			else if (auto jump = std::get_if<IRStatementJump>(&last->expr))
			{
				AddEdge(
					block.id,
					labelToBlock[jump->label]
				);
			}
			else if (auto cjump = std::get_if<IRStatementCJump>(&last->expr))
			{
				AddEdge(block.id, labelToBlock[cjump->labelTrue]);
				AddEdge(block.id, labelToBlock[cjump->labelFalse]);
			}
		}
		*/
		return program;
	}
private:

	void AddEdge(size_t id, size_t block)
	{
		graph.edges.emplace_back(id, block);
	}
	void V(Instruction* ins)
	{
		if (std::holds_alternative<TACCJump>(ins->expr) || std::holds_alternative<TACJump>(ins->expr))
		{
			currentBlock.ins.push_back(ins);
			if (currentBlock.label.empty()) {
				currentBlock.label = "B" + std::to_string(block_counter++);
			}
			FinishBlock();
		}
		else if (auto label = std::get_if<TACLabel>(&ins->expr))
		{
			currentBlock.ins.push_back(ins);
			FinishBlock();
			currentBlock.label = label->label;
		}
		else
		{
			currentBlock.ins.push_back(ins);

		}

	}

	void FinishBlock()
	{
		if (currentBlock.ins.empty() && currentBlock.label.empty())
			return;

		currentBlock.id = block_counter++;

		if (!currentBlock.label.empty())
			labelToBlock[currentBlock.label] = currentBlock.id;

		graph.blocks.push_back(std::move(currentBlock));
		currentBlock = BasicBlock{};
	}
	std::pmr::polymorphic_allocator<Instruction> alloc;
	BasicBlock currentBlock = {};
	int block_counter = 0;
	std::vector<BasicBlock> blocks;

	const TACProgram& instructions;
	std::unordered_map<std::string, size_t> labelToBlock;

	CFG graph;
};

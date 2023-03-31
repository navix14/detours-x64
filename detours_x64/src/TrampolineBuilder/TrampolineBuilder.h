#pragma once

#include <vector>
#include <cstdint>
#include <set>

#include "lib/Zydis/Zydis.h"
#include "ZydisUtils/ZydisUtils.h"
#include "AnnotatedInstruction/AnnotatedInstruction.h"

class TrampolineBuilder {
private:
	ZydisDecoder decoder;
	ZydisFormatter formatter;
	ZydisUtils zydis_utils;

	std::vector<AnnotatedInstruction> annotated_instructions;

	uintptr_t cave_address;
	uintptr_t jump_table_address;
	uintptr_t address_table_address;

public:
	TrampolineBuilder(void* original_address, const size_t stolen_size, void* cave_address);

	void* get_jump_back_ptr();

	void* get_jump_to_hook_ptr();

	void build(void* hook_function);

private:
	void initialize_tables(uintptr_t original_address, const size_t stolen_size);

	static void place_jump(void* from, void* to);

	std::vector<uint8_t> rewrite_instruction(const AnnotatedInstruction& instruction, uintptr_t& runtime_address);

	void place_relocation(void* to);

	void place_qword_jump(void* from, void* to);

	void build_annotated_instructions(uintptr_t address, const uint8_t* buffer, const size_t buffer_size);

	ZydisRegister fit_register(ZydisRegister reg, ZydisRegister other_reg);

	ZydisRegister instruction_get_unused_register(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]);
};
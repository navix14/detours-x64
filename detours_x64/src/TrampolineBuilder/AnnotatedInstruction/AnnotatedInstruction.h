#pragma once

#include <cstdint>

#include "lib/Zydis/Zydis.h"

class AnnotatedInstruction {
private:
	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[10];

	bool is_relative;
	size_t relative_operand_id;
	uintptr_t absolute_address;

public:
	AnnotatedInstruction(uintptr_t instruction_address, const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]);

	const ZydisDecodedOperand* get_operands() const;

	bool get_is_relative() const;

	size_t get_relative_operand_id() const;

	uintptr_t get_absolute_address() const;

	ZydisDecodedInstruction get_raw() const;
};
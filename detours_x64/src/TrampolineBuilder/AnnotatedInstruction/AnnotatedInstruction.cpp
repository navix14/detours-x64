#include "AnnotatedInstruction.h"

AnnotatedInstruction::AnnotatedInstruction(uintptr_t instruction_address, const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]) {
	this->instruction = instruction;
	this->is_relative = instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE;
	this->absolute_address = 0;
	this->relative_operand_id = 0;

	for (int i = 0; i < instruction.operand_count_visible; i++) {
		const auto operand = operands[i];

		this->operands[i] = operand;

		if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY) {
			this->relative_operand_id = operand.id;
			this->absolute_address = (instruction_address + instruction.length) + operand.mem.disp.value;
		}
	}
}

const ZydisDecodedOperand* AnnotatedInstruction::get_operands() const {
	return this->operands;
}

bool AnnotatedInstruction::get_is_relative() const {
	return this->is_relative;
}

size_t AnnotatedInstruction::get_relative_operand_id() const {
	return this->relative_operand_id;
}

uintptr_t AnnotatedInstruction::get_absolute_address() const {
	return this->absolute_address;
}

ZydisDecodedInstruction AnnotatedInstruction::get_raw() const {
	return instruction;
}
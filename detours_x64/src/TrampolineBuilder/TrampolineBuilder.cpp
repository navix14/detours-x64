#include "TrampolineBuilder.h"

TrampolineBuilder::TrampolineBuilder(void* original_address, const size_t stolen_size, void* cave_address) {
	this->cave_address = (uintptr_t)cave_address;

	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

	build_annotated_instructions((uintptr_t)original_address, (uint8_t*)original_address, stolen_size);
	initialize_tables((uintptr_t)original_address, stolen_size);
}

void* TrampolineBuilder::get_jump_back_ptr() {
	return (void*)(cave_address + 0x50);
}

void* TrampolineBuilder::get_jump_to_hook_ptr() {
	return (void*)(cave_address + 0x58);
}

void TrampolineBuilder::build(void* hook_function) {
	place_relocation(hook_function);

	std::vector<uint8_t> bytes;
	uintptr_t runtime_address = cave_address;

	for (const auto& instruction : annotated_instructions) {
		const auto& instruction_bytes = rewrite_instruction(instruction, runtime_address);

		for (auto byte : instruction_bytes) {
			bytes.push_back(byte);
		}
	}

	std::memcpy((void*)cave_address, bytes.data(), bytes.size());
	place_jump((uint8_t*)cave_address + bytes.size(), get_jump_back_ptr());
}

void TrampolineBuilder::initialize_tables(uintptr_t original_address, const size_t stolen_size) {
	jump_table_address = cave_address + 0x50;
	address_table_address = cave_address + 0x100;

	place_relocation((void*)(original_address + stolen_size)); // jump back ptr
}

void TrampolineBuilder::place_jump(void* from, void* to) {
	*reinterpret_cast<uint8_t*>(from) = 0xE9;
	*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(from) + 1) = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(to) - (reinterpret_cast<uint8_t*>(from) + 5));
}

std::vector<uint8_t> TrampolineBuilder::rewrite_instruction(const AnnotatedInstruction& instruction, uintptr_t& runtime_address) {
	std::vector<uint8_t> rewritten_bytes;

	const auto& raw_instruction = instruction.get_raw();
	const ZydisDecodedOperand* operands = instruction.get_operands();
	const auto mnemonic = raw_instruction.mnemonic;

	if (!instruction.get_is_relative()) {
		size_t size = 0;
		const auto bytes = zydis_utils.encode(raw_instruction, operands, size);
		runtime_address += size;
		return bytes;
	}

	if (zydis_utils.is_jump(mnemonic) || mnemonic == ZYDIS_MNEMONIC_CALL) {
		place_relocation((void*)instruction.get_absolute_address());
		uintptr_t target = jump_table_address - sizeof(uintptr_t);

		// Rewrite JCC/CALL
		ZydisEncoderRequest req;
		ZydisEncoderDecodedInstructionToEncoderRequest(&raw_instruction, operands, raw_instruction.operand_count_visible, &req);
		req.operands[0].type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
		req.operands[0].imm.s = target - (runtime_address + raw_instruction.length);

		size_t size;
		const auto rewritten_insn = zydis_utils.encode(req, size);
		rewritten_bytes.insert(rewritten_bytes.end(), rewritten_insn.begin(), rewritten_insn.end());

		runtime_address += size;
	} else {
		const auto unused_reg = instruction_get_unused_register(raw_instruction, operands);

		const size_t relative_op_id = instruction.get_relative_operand_id();
		const size_t other_op_id = relative_op_id == 0 ? 1 : 0;

		ZydisEncoderRequest req;
		ZydisEncoderDecodedInstructionToEncoderRequest(&raw_instruction, operands, raw_instruction.operand_count_visible, &req);
		req.operands[relative_op_id].type = ZYDIS_OPERAND_TYPE_REGISTER;
		req.operands[relative_op_id].reg.value = fit_register(unused_reg, req.operands[other_op_id].reg.value);

		size_t size = 0;

		auto rewritten_insn = zydis_utils.encode_push_reg(unused_reg, size);
		rewritten_bytes.insert(rewritten_bytes.end(), rewritten_insn.begin(), rewritten_insn.end());
		runtime_address += size;

		rewritten_insn = zydis_utils.encode_mov_reg_mem(unused_reg, instruction.get_absolute_address(), size);
		rewritten_bytes.insert(rewritten_bytes.end(), rewritten_insn.begin(), rewritten_insn.end());
		runtime_address += size;

		rewritten_insn = zydis_utils.encode(req, size);
		rewritten_bytes.insert(rewritten_bytes.end(), rewritten_insn.begin(), rewritten_insn.end());
		runtime_address += size;

		rewritten_insn = zydis_utils.encode_pop_reg(unused_reg, size);
		rewritten_bytes.insert(rewritten_bytes.end(), rewritten_insn.begin(), rewritten_insn.end());
		runtime_address += size;
	}

	return rewritten_bytes;
}

void TrampolineBuilder::place_relocation(void* to) {
	*reinterpret_cast<uintptr_t*>(address_table_address) = (uintptr_t)to;
	place_qword_jump((void*)jump_table_address, (void*)address_table_address);

	jump_table_address += sizeof(uintptr_t);
	address_table_address += sizeof(uintptr_t);
}

void TrampolineBuilder::place_qword_jump(void* from, void* to) {
	uint8_t jump_qword[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
	size_t disp = (size_t)to - ((size_t)from + sizeof(jump_qword));

	*(uint32_t*)(jump_qword + 2) = static_cast<uint32_t>(disp);

	std::memcpy(from, jump_qword, sizeof(jump_qword));
}

void TrampolineBuilder::build_annotated_instructions(uintptr_t address, const uint8_t* buffer, const size_t buffer_size) {
	// First pass: Build annotated instructions
	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

	size_t offset = 0;

	while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buffer + offset, buffer_size - offset, &instruction, operands))
		&& offset < buffer_size) {
		AnnotatedInstruction annotated_instruction(address + offset, instruction, operands);
		annotated_instructions.push_back(annotated_instruction);

		offset += instruction.length;
	}
}

ZydisRegister TrampolineBuilder::fit_register(ZydisRegister reg, ZydisRegister other_reg) {
	// `reg` is always 64-bit wide
	const auto width_other = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, other_reg);

	if (width_other == 32) {
		return (ZydisRegister)((size_t)reg - 16);
	}
	else if (width_other == 16) {
		return (ZydisRegister)((size_t)reg - 32);
	}
	else if (width_other == 8) {
		return (ZydisRegister)((size_t)reg - 48);
	}

	return reg;
}

ZydisRegister TrampolineBuilder::instruction_get_unused_register(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]) {
	std::set<ZydisRegister> regs = {
		ZYDIS_REGISTER_RAX,
		ZYDIS_REGISTER_RBX,
		ZYDIS_REGISTER_RCX,
		ZYDIS_REGISTER_RDX,
		ZYDIS_REGISTER_RSI,
		ZYDIS_REGISTER_RDI,
		ZYDIS_REGISTER_RBP,
		ZYDIS_REGISTER_R8,
		ZYDIS_REGISTER_R9,
		ZYDIS_REGISTER_R10,
		ZYDIS_REGISTER_R11,
		ZYDIS_REGISTER_R12,
		ZYDIS_REGISTER_R13,
		ZYDIS_REGISTER_R14,
		ZYDIS_REGISTER_R15,
	};

	std::set<ZydisRegister> used_regs = {};

	for (int i = 0; i < instruction.operand_count; i++) {
		const auto& operand = operands[i];

		if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER) {
			used_regs.insert(operand.reg.value);
		}
	}

	for (const auto& reg : regs) {
		if (!used_regs.contains(reg)) {
			return reg;
		}
	}

	return ZydisRegister::ZYDIS_REGISTER_MAX_VALUE;
}
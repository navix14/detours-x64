#include "ZydisUtils.h"

#include <vector>

ZydisUtils::ZydisUtils() {
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
}

bool ZydisUtils::is_jump(ZydisMnemonic mnemonic) {
	return mnemonic <= 320 && mnemonic >= 299;
}

std::vector<uint8_t> ZydisUtils::encode(ZydisEncoderRequest req, size_t& size) {
	uint8_t encoded_instruction[ZYDIS_MAX_INSTRUCTION_LENGTH];
	size_t encoded_length = sizeof(encoded_instruction);

	if (ZYAN_FAILED(ZydisEncoderEncodeInstruction(&req, encoded_instruction, &encoded_length))) {
		std::printf("Failed to encode instruction\n");
		return { };
	}

	std::vector<uint8_t> bytes;

	for (int i = 0; i < encoded_length; i++) {
		bytes.push_back(encoded_instruction[i]);
	}

	size = encoded_length;

	return bytes;
}

std::vector<uint8_t> ZydisUtils::encode(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[], size_t& size) {
	ZydisEncoderRequest req;

	if (ZYAN_FAILED(ZydisEncoderDecodedInstructionToEncoderRequest(&instruction, operands, instruction.operand_count_visible, &req))) {
		std::printf("Failed to create encoder request\n");
		return { };
	}

	const auto encoded = encode(req, size);

	return encoded;
}

std::vector<uint8_t> ZydisUtils::encode_push_reg(ZydisRegister reg, size_t& size) {
	ZydisEncoderRequest req;
	memset(&req, 0, sizeof(req));

	req.mnemonic = ZYDIS_MNEMONIC_PUSH;
	req.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
	req.operand_count = 1;
	req.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
	req.operands[0].reg.value = reg;

	return encode(req, size);
}

std::vector<uint8_t> ZydisUtils::encode_pop_reg(ZydisRegister reg, size_t& size) {
	ZydisEncoderRequest req;
	memset(&req, 0, sizeof(req));

	req.mnemonic = ZYDIS_MNEMONIC_POP;
	req.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
	req.operand_count = 1;
	req.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
	req.operands[0].reg.value = reg;

	return encode(req, size);
}

std::vector<uint8_t> ZydisUtils::encode_mov_reg_mem(ZydisRegister reg, uintptr_t memory_address, size_t& size) {
	ZydisEncoderRequest req;
	memset(&req, 0, sizeof(req));

	req.mnemonic = ZYDIS_MNEMONIC_MOV;
	req.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
	req.operand_count = 2;

	req.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
	req.operands[0].reg.value = reg;

	req.operands[1].type = ZYDIS_OPERAND_TYPE_MEMORY;
	req.operands[1].mem.base = ZYDIS_REGISTER_NONE;
	req.operands[1].mem.displacement = memory_address;
	req.operands[1].mem.size = sizeof(uintptr_t);

	return encode(req, size);
}

std::string ZydisUtils::encode_and_format(const ZydisEncoderRequest& req, size_t& size) {
	uint8_t encoded_instruction[ZYDIS_MAX_INSTRUCTION_LENGTH];
	size_t encoded_length = sizeof(encoded_instruction);

	// 1. Encode
	if (ZYAN_FAILED(ZydisEncoderEncodeInstruction(&req, encoded_instruction, &encoded_length))) {
		std::printf("Failed to encode instruction\n");
		return "";
	}

	ZydisDecodedInstruction decoded_instruction;
	ZydisDecodedOperand decoded_operands[ZYDIS_MAX_OPERAND_COUNT];

	// 2. Decode
	if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, encoded_instruction, encoded_length, &decoded_instruction, decoded_operands))) {
		std::printf("Failed to decode instruction\n");
		return "";
	}

	// 3. Format
	char buffer[256] = { 0 };

	if (ZYAN_FAILED(ZydisFormatterFormatInstruction(&formatter, &decoded_instruction, decoded_operands,
		decoded_instruction.operand_count, buffer, sizeof(buffer), ZYDIS_RUNTIME_ADDRESS_NONE, ZYAN_NULL))) {
		std::printf("Failed to format instruction\n");
		return "";
	}

	size = encoded_length;

	return std::string(buffer);
}

std::string ZydisUtils::to_string(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]) {
	ZydisEncoderRequest req;
	size_t size;
	ZydisEncoderDecodedInstructionToEncoderRequest(&instruction, operands, instruction.operand_count_visible, &req);

	return encode_and_format(req, size);
}
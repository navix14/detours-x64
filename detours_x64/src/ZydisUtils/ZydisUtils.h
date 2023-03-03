#pragma once

#include <string>
#include <vector>

#include "lib/Zydis/Zydis.h"

class ZydisUtils {
private:
	ZydisFormatter formatter;
	ZydisDecoder decoder;

public:
	ZydisUtils();

	bool is_jump(ZydisMnemonic mnemonic);

	std::vector<uint8_t> encode(ZydisEncoderRequest req, size_t& size);

	std::vector<uint8_t> encode(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[], size_t& size);

	std::vector<uint8_t> encode_push_reg(ZydisRegister reg, size_t& size);

	std::vector<uint8_t> encode_pop_reg(ZydisRegister reg, size_t& size);

	std::vector<uint8_t> encode_mov_reg_mem(ZydisRegister reg, uintptr_t memory_address, size_t& size);

	std::string encode_and_format(const ZydisEncoderRequest& req, size_t& size);

	std::string to_string(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand operands[]);
};
#pragma once

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <Windows.h>

#include "lib/Zydis/Zydis.h"

#include "SystemInfo/SystemInfo.h"
#include "EnumMappings/EnumMappings.h"
#include "TrampolineBuilder/TrampolineBuilder.h"

struct hook {
	void* trampoline;
	std::vector<byte> original_bytes;
};

class HookLib {
private:
	std::unordered_map<void*, hook> active_hooks;

public:
	template <typename Fn>
	Fn apply_hook_x86(void* original_function, void* target_function) {
		size_t size = compute_hook_size(original_function, 5);

		void* trampoline = VirtualAlloc(nullptr, size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

		if (trampoline == nullptr) {
			std::wcout << L"[error] failed to allocate memory for trampoline: " << SystemInfo::last_error_string() << std::endl;
			return nullptr;
		}

		std::memcpy(trampoline, original_function, size);
		place_jump((uint8_t*)trampoline + size, (uint8_t*)original_function + size);

		relocate(trampoline, original_function, size);

		ensure_protection(original_function, size, PAGE_EXECUTE_READWRITE, [=]() {
			place_jump(original_function, target_function);
			std::memset((uint8_t*)original_function + size, 0x90, size - 5);
		});
	}

	template <typename Fn>
	Fn apply_hook_x64(void* original_function, void* target_function) {
		bool use_far_jump = false;
		size_t size = 0;

		void* trampoline = allocate_around_2gb(original_function);

		if (trampoline == nullptr) {
			trampoline = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			use_far_jump = true;
		}

		if (trampoline == nullptr) {
			std::wcout << L"[error] failed to allocate memory for trampoline: " << SystemInfo::last_error_string() << std::endl;
			return nullptr;
		}

		if (use_far_jump) {
			size = compute_hook_size(original_function, 14);

			TrampolineBuilder trampoline_builder(original_function, size, trampoline);
			trampoline_builder.build(target_function);

			const auto jump_back_ptr = trampoline_builder.get_jump_back_ptr();
			const auto jump_to_hook_ptr = trampoline_builder.get_jump_to_hook_ptr();

			// Far jump to jump_back_ptr
			// FF25 00000000 0000A7B90C020000 - jmp 20CB9A70000
			const uint8_t far_jump[] = { 0xFF, 0x25,
										 0x00, 0x00, 0x00, 0x00,
										 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

			*(uintptr_t*)(far_jump + 6) = (uintptr_t)jump_to_hook_ptr;

			ensure_protection(original_function, size, PAGE_EXECUTE_READWRITE, [=]() {
				std::memcpy(original_function, far_jump, sizeof(far_jump));
				std::memset(original_function, 0x90, size - sizeof(far_jump));
			});

		} else {
			size = compute_hook_size(original_function, 5);

			std::memcpy(trampoline, original_function, size);
			place_jump((byte*)trampoline + size, (byte*)original_function + size);
			place_qword_jump((byte*)trampoline + size + 5, (byte*)trampoline + 0x50);
			*(uintptr_t*)((byte*)trampoline + 0x50) = (uintptr_t)target_function;

			relocate(trampoline, original_function, size);

			ensure_protection(original_function, size, PAGE_EXECUTE_READWRITE, [=]() {
				place_jump(original_function, (byte*)trampoline + size + 5);
				std::memset((byte*)original_function + 5, 0x90, size - 5);
			});
		}

		active_hooks.insert(std::make_pair(original_function, create_hook_entry(trampoline, size)));

		return reinterpret_cast<Fn>(trampoline);
	}

	void remove_hook(void* original_function) {
		auto it = active_hooks.find(original_function);

		if (it != active_hooks.end()) {
			const auto& hook = it->second;
			const auto address = it->first;

			size_t num_bytes = hook.original_bytes.size();

			ensure_protection(address, num_bytes, PAGE_EXECUTE_READWRITE, [=]() {
				memcpy(address, hook.original_bytes.data(), num_bytes);
			});

			if (!VirtualFree(hook.trampoline, 0, MEM_RELEASE)) {
				std::wcout << "[error] failed to deallocate trampoline at " << std::hex << hook.trampoline << std::endl;
			}

			active_hooks.erase(it);
		}
	}

private:
	static hook create_hook_entry(void* trampoline, size_t size) {
		std::vector<byte> original_bytes(size);
		std::memcpy(original_bytes.data(), trampoline, size);

		return { trampoline, original_bytes };
	}

	static size_t compute_hook_size(const void* address, size_t needed_size) {
		ZyanUSize offset = 0;
		ZydisDisassembledInstruction instruction;

		while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			ZYDIS_MACHINE_MODE_LONG_64,
			0x0,
			static_cast<const byte*>(address) + offset,
			0x1000,
			&instruction
		))) {
			if (offset >= needed_size) {
				break;
			}

			offset += instruction.info.length;
		};

		return offset;
	}

	static void place_jump(void* from, void* to) {
		*reinterpret_cast<byte*>(from) = 0xE9;
		*reinterpret_cast<uint32_t*>(reinterpret_cast<byte*>(from) + 1) = static_cast<uint32_t>(reinterpret_cast<byte*>(to) - (reinterpret_cast<byte*>(from) + 5));
	}

	static void place_qword_jump(void* from, void* to) {
		byte jump_qword[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
		size_t disp = (size_t)to - ((size_t)from + sizeof(jump_qword));

		*(uint32_t*)(jump_qword + 2) = static_cast<uint32_t>(disp);

		std::memcpy(from, jump_qword, sizeof(jump_qword));
	}

	static void* allocate_around_2gb(const void* base_address, const size_t size = 0x500) {
		const auto base = reinterpret_cast<uintptr_t>(base_address);
		const auto num_pages_required = static_cast<size_t>(std::ceil((double)size / SystemInfo::page_size()));

		// Align to allocation boundary for VirtualQuery
		const auto base_aligned = base - (base % SystemInfo::allocation_granularity());

		// Maximum allocation address
		const auto high_bound = base + (1ULL << 31) - size;
		const auto low_bound = base - (1ULL << 31) +  size;

		// Start searching from the next 64 KB region, since VirtualAlloc
		// reserves 64 KB-aligned regions with MEM_RESERVE and non-NULL `lpAddress`
		auto curr = base_aligned + SystemInfo::allocation_granularity();

		bool found = false;

		// Search for higher addresses, left out the backwards search for clarity
		while (curr < high_bound) {
			MEMORY_BASIC_INFORMATION mbi;
			std::memset(&mbi, 0, sizeof(mbi));

			if (VirtualQuery(reinterpret_cast<void*>(curr), &mbi, sizeof(mbi))) {
				// If we find a MEM_FREE region with enough space, use it
				if ((mbi.State & MEM_FREE) && mbi.RegionSize >= (num_pages_required * SystemInfo::page_size())) {
					found = true;
					break;
				}
			}

			// `curr` is always aligned to the allocation granularity (64 KB)
			curr += SystemInfo::allocation_granularity();
		}

		if (!found) {
			curr = base_aligned - SystemInfo::allocation_granularity();

			// Search for lower addresses
			while (curr > low_bound) {
				MEMORY_BASIC_INFORMATION mbi;
				std::memset(&mbi, 0, sizeof(mbi));

				if (VirtualQuery(reinterpret_cast<void*>(curr), &mbi, sizeof(mbi))) {
					// If we find a MEM_FREE region with enough space, use it
					if ((mbi.State & MEM_FREE) && mbi.RegionSize >= (num_pages_required * SystemInfo::page_size())) {
						found = true;
						break;
					}
				}

				// `curr` is always aligned to the allocation granularity (64 KB)
				curr -= SystemInfo::allocation_granularity();
			}
		}

		if (!found) {
			return nullptr;
		}

		return VirtualAlloc(reinterpret_cast<void*>(curr), size,
			MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	}

	static bool relocate(void* trampoline_base, void* original_base, size_t size) {
		const auto delta = (uintptr_t)original_base - (uintptr_t)trampoline_base;
		const auto abs_delta = std::abs((long long)original_base - (long long)trampoline_base);

		if (abs_delta > (1LL << 31)) {
			std::printf("[error] failed to relocate instructions: 64-bit displacements are not supported\n");
			return false;
		}

		ZydisMachineMode machine_mode = ZYDIS_MACHINE_MODE_LEGACY_32;

#ifdef _WIN64
		machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
#endif

		ZydisDisassembledInstruction instruction;
		size_t offset = 0;
		while (ZYAN_SUCCESS(ZydisDisassembleIntel(
			machine_mode,
			0,
			(byte*)trampoline_base + offset,
			size - offset,
			&instruction)) && offset < size) {
			
			if (instruction.info.attributes & ZYDIS_ATTRIB_IS_RELATIVE) {
				auto current_instruction_ptr = reinterpret_cast<uintptr_t>(trampoline_base) + offset;

				auto original_disp = instruction.info.raw.disp.value;
				auto disp_size = instruction.info.raw.disp.size;
				auto disp_offset = instruction.info.raw.disp.offset;

				if (disp_size > 32) {
					std::printf("[error] failed to relocate instruction: %s\n", instruction.text);
					return false;
				}

				auto new_disp = original_disp + delta;

				switch (disp_size) {
				case 8:
					*reinterpret_cast<uint8_t*>(current_instruction_ptr + disp_offset) = static_cast<uint8_t>(new_disp);
					break;
				case 16:
					*reinterpret_cast<uint16_t*>(current_instruction_ptr + disp_offset) = static_cast<uint16_t>(new_disp);
					break;
				case 32:
					*reinterpret_cast<uint32_t*>(current_instruction_ptr + disp_offset) = static_cast<uint32_t>(new_disp);
					break;
				}
			}

			offset += instruction.info.length;
		}

		return true;
	}

	template <typename Lambda>
	static void ensure_protection(void* address, size_t size, uint32_t new_protection, Lambda action) {
		DWORD old_protection;

		VirtualProtect(address, size, new_protection, &old_protection);
		action();
		VirtualProtect(address, size, old_protection, &old_protection);
	}
};
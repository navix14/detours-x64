#pragma once

#include "EnumMappings.h"

#include <Windows.h>
#include <vector> 

namespace Winternals {
	std::unordered_map<uint32_t, std::string> memory_region_property_map = {
		{MEM_COMMIT, "MEM_COMMIT"},
		{MEM_RESERVE, "MEM_RESERVE"},
		{MEM_DECOMMIT, "MEM_DECOMMIT"},
		{MEM_RELEASE, "MEM_RELEASE"},
		{MEM_FREE, "MEM_FREE"},
		{MEM_RESERVE_PLACEHOLDER, "MEM_RESERVE_PLACEHOLDER"},
		{MEM_RESET, "MEM_RESET"},
		{MEM_TOP_DOWN, "MEM_TOP_DOWN"},
		{MEM_WRITE_WATCH, "MEM_WRITE_WATCH"},
		{MEM_PHYSICAL, "MEM_PHYSICAL"},
		{MEM_ROTATE, "MEM_ROTATE"},
		{MEM_DIFFERENT_IMAGE_BASE_OK, "MEM_DIFFERENT_IMAGE_BASE_OK"},
		{MEM_RESET_UNDO, "MEM_RESET_UNDO"},
		{MEM_LARGE_PAGES, "MEM_LARGE_PAGES"},
		{MEM_4MB_PAGES, "MEM_4MB_PAGES"}
	};

	std::unordered_map<uint32_t, std::string> page_property_map{
		{PAGE_NOACCESS, "PAGE_NOACCESS"},
		{PAGE_READONLY, "PAGE_READONLY"},
		{PAGE_READWRITE, "PAGE_READWRITE"},
		{PAGE_WRITECOPY, "PAGE_WRITECOPY"},
		{PAGE_EXECUTE, "PAGE_EXECUTE"},
		{PAGE_EXECUTE_READ, "PAGE_EXECUTE_READ"},
		{PAGE_EXECUTE_READWRITE, "PAGE_EXECUTE_READWRITE"},
		{PAGE_EXECUTE_WRITECOPY, "PAGE_EXECUTE_WRITECOPY"},
		{PAGE_GUARD, "PAGE_GUARD"},
		{PAGE_NOCACHE, "PAGE_NOCACHE"},
		{PAGE_WRITECOMBINE, "PAGE_WRITECOMBINE"},
		{PAGE_GRAPHICS_NOACCESS, "PAGE_GRAPHICS_NOACCESS"},
		{PAGE_GRAPHICS_READONLY, "PAGE_GRAPHICS_READONLY"},
		{PAGE_GRAPHICS_READWRITE, "PAGE_GRAPHICS_READWRITE"},
		{PAGE_GRAPHICS_EXECUTE, "PAGE_GRAPHICS_EXECUTE"},
		{PAGE_GRAPHICS_EXECUTE_READ, "PAGE_GRAPHICS_EXECUTE_READ"},
		{PAGE_GRAPHICS_EXECUTE_READWRITE, "PAGE_GRAPHICS_EXECUTE_READWRITE"},
		{PAGE_GRAPHICS_COHERENT, "PAGE_GRAPHICS_COHERENT"},
		{PAGE_GRAPHICS_NOCACHE, "PAGE_GRAPHICS_NOCACHE"},
		{PAGE_ENCLAVE_THREAD_CONTROL, "PAGE_ENCLAVE_THREAD_CONTROL"},
		{PAGE_REVERT_TO_FILE_MAP, "PAGE_REVERT_TO_FILE_MAP"},
		{PAGE_TARGETS_NO_UPDATE, "PAGE_TARGETS_NO_UPDATE"},
		{PAGE_TARGETS_INVALID, "PAGE_TARGETS_INVALID"},
		{PAGE_ENCLAVE_UNVALIDATED, "PAGE_ENCLAVE_UNVALIDATED"},
		{PAGE_ENCLAVE_MASK, "PAGE_ENCLAVE_MASK"},
	};

	std::string lookup(const std::unordered_map<uint32_t, std::string>& map, uint32_t bitmask) {
		std::vector<std::string> active_flags;

		for (const auto& entry : map) {
			if (bitmask & entry.first) {
				active_flags.push_back(entry.second);
			}
		}

		std::string result = "";

		for (int i = 0; i < active_flags.size(); i++) {
			result += active_flags[i];

			if (i != 0 && i != active_flags.size() - 1) {
				result += " | ";
			}
		}

		return result;
	}

	std::string page_property_string(uint32_t page_property) {
		return lookup(page_property_map, page_property);
	}

	std::string memory_region_state_string(uint32_t memory_region_state) {
		return lookup(memory_region_property_map, memory_region_state);
	}
}
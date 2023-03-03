#pragma once

#include <unordered_map>
#include <string>

namespace Winternals {
	std::string page_property_string(uint32_t page_property);
	std::string memory_region_state_string(uint32_t memory_region_state);
}
#pragma once
#include <Windows.h>
#include <cstdint>

class SystemInfo {
private:
	static uint32_t m_allocation_granularity;
	static uint32_t m_page_size;

public:
	static uint32_t allocation_granularity();
	static uint32_t page_size();
	static std::wstring last_error_string();

private:
	static void init();
};
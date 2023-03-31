#include <Windows.h>
#include <cstdint>
#include <string>

#include "SystemInfo.h"

void SystemInfo::init() {
	SYSTEM_INFO system_info;
	memset(&system_info, 0, sizeof(SYSTEM_INFO));
	GetSystemInfo(&system_info);

	m_allocation_granularity = system_info.dwAllocationGranularity;
	m_page_size = system_info.dwPageSize;
}

uint32_t SystemInfo::allocation_granularity() {
	if (m_allocation_granularity == 0) {
		init();
	}

	return m_allocation_granularity;
}

uint32_t SystemInfo::page_size() {
	if (m_page_size == 0) {
		init();
	}

	return m_page_size;
}

std::wstring SystemInfo::last_error_string() {
	auto error_code = GetLastError();
	wchar_t* message_buffer = nullptr;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&message_buffer,
		0,
		nullptr
	);

	return std::wstring(message_buffer);
}


uint32_t SystemInfo::m_allocation_granularity = 0;
uint32_t SystemInfo::m_page_size = 0;
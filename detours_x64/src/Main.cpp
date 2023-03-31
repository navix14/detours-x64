#include <iostream>
#include <Windows.h>
#include <chrono>
#include <thread>

#include "../lib/Zydis/Zydis.h"

#include "HookLib/HookLib.h"
#include "TrampolineBuilder/TrampolineBuilder.h"

using namespace std::chrono_literals;

using type_messageboxa_x64 = int(__fastcall*)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
type_messageboxa_x64 original_messageboxa_x64 = nullptr;

int __fastcall MessageBoxA_hook_x64(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
	return original_messageboxa_x64(hWnd, "hooked", "hooked", uType);
}

void start(void* parameter) {
	auto hook_lib = HookLib();
	original_messageboxa_x64 = hook_lib.apply_hook_x64<type_messageboxa_x64>(&MessageBoxA, &MessageBoxA_hook_x64);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(start), nullptr, 0, nullptr);
	}

	return TRUE;
}
#include <windows.h>
#include <format>
#include "toml++/toml.hpp"

#include "nya_commonhooklib.h"

#include "fouc.h"
#include "fo2versioncheck.h"
#include "chloemenulib.h"

void ValueEditorMenu(float& value) {
	ChloeMenuLib::BeginMenu();

	static char inputString[1024] = {};
	ChloeMenuLib::AddTextInputToString(inputString, 1024, true);
	ChloeMenuLib::SetEnterHint("Apply");

	if (DrawMenuOption(inputString + (std::string)"...", "", false, false) && inputString[0]) {
		value = std::stof(inputString);
		memset(inputString,0,sizeof(inputString));
		ChloeMenuLib::BackOut();
	}

	ChloeMenuLib::EndMenu();
}

bool bBouncyEnabled = true;
bool bBouncyEnabledForPlayer = false;
bool bBouncyEnabledForCarsOnly = false;
float fBouncyScale = 2.5;
float fCurrentBouncyScale = 1.0;

void MenuLoop() {
	ChloeMenuLib::BeginMenu();

	if (DrawMenuOption(std::format("Bouncy - {}", bBouncyEnabled))) {
		bBouncyEnabled = !bBouncyEnabled;
	}

	if (DrawMenuOption(std::format("Bounciness - {}", fBouncyScale))) {
		ValueEditorMenu(fBouncyScale);
	}

	if (DrawMenuOption(std::format("Affect Player Car - {}", bBouncyEnabledForPlayer))) {
		bBouncyEnabledForPlayer = !bBouncyEnabledForPlayer;
	}

	if (DrawMenuOption(std::format("Car-to-Car Only - {}", bBouncyEnabledForCarsOnly))) {
		bBouncyEnabledForCarsOnly = !bBouncyEnabledForCarsOnly;
	}

	ChloeMenuLib::EndMenu();
}

bool IsObjectAPlayer(Car* pCar) {
	for (int i = 0; i < 32; i++) {
		auto ply = GetPlayer(i);
		if (!ply) continue;
		if (pCar == ply->pCar) return true;
	}
	return false;
}

void __fastcall BouncyCarCheck(Car* pCar, void* origPtr, Car* pTarget) {
	fCurrentBouncyScale = 1.0;
	if (!origPtr) return;

	fCurrentBouncyScale = fBouncyScale;
	if (!bBouncyEnabledForPlayer && pCar == GetPlayer(0)->pCar) fCurrentBouncyScale = 1.0;
	if (bBouncyEnabledForCarsOnly && (!IsObjectAPlayer(pCar) || !IsObjectAPlayer(pTarget))) fCurrentBouncyScale = 1.0;
}

// skip writing to the damage value for remote players
uintptr_t BouncyCarCheckASM1_jmp = 0x5D631E;
float __attribute__((naked)) BouncyCarCheckASM1() {
	__asm__ (
		"pushad\n\t"

		// target
		"mov eax, [ecx+4]\n\t"
		"sub eax, 0x1C0\n\t"
		"push eax\n\t"

		"mov eax, [ebx+0x10]\n\t"
		"mov ecx, [eax]\n\t"
		"sub ecx, 0x1C0\n\t"
		"mov edx, [eax]\n\t"
		"call %1\n\t"
		"popad\n\t"

		"jmp %0\n\t"
			:
			:  "m" (BouncyCarCheckASM1_jmp), "i" (BouncyCarCheck)
	);
}

// skip writing to the damage value for remote players
uintptr_t BouncyCarCheckASM2_jmp = 0x5D63B9;
float __attribute__((naked)) BouncyCarCheckASM2() {
	__asm__ (
		"mov eax, [ecx+4]\n\t"
		"pushad\n\t"

		// previous target
		"mov ebx, [ebx+0x10]\n\t"
		"mov ecx, [ebx]\n\t"
		"sub ecx, 0x1C0\n\t"
		"push ecx\n\t"

		"mov ecx, eax\n\t"
		"sub ecx, 0x1C0\n\t"
		"mov edx, eax\n\t"
		"call %1\n\t"
		"popad\n\t"

		"test eax, eax\n\t"
		"jmp %0\n\t"
			:
			:  "m" (BouncyCarCheckASM2_jmp), "i" (BouncyCarCheck)
	);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			DoFlatOutVersionCheck(FO2Version::FOUC_GFWL);

			ChloeMenuLib::RegisterMenu("Bouncy Mod - gaycoderprincess", MenuLoop);
			NyaFO2Hooks::PlaceD3DHooks();

			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x5D6310, &BouncyCarCheckASM1);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x5D63B4, &BouncyCarCheckASM2);
			NyaHookLib::Patch(0x5D631E + 2, &fCurrentBouncyScale);
			NyaHookLib::Patch(0x5D63CF + 2, &fCurrentBouncyScale);
			//NyaHookLib::Patch<uint8_t>(0x5D6314, 0xEB);
			NyaHookLib::Patch<uint8_t>(0x5D63C5, 0xEB);

			auto config = toml::parse_file("FlatOutUCBouncyMod_gcp.toml");
			bBouncyEnabled = config["main"]["enabled"].value_or(true);
			fBouncyScale = config["main"]["bounciness"].value_or(2.5);
			bBouncyEnabledForPlayer = config["main"]["affect_player"].value_or(false);
		} break;
		default:
			break;
	}
	return TRUE;
}
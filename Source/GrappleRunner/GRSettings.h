// =====================================================================================
//  GRSettings.h  -  the player's settings, saved between games
// =====================================================================================
//
//  WHAT THIS IS
//  A tiny helper the menus and the game share. Settings are saved in Unreal's own
//  "GameUserSettings.ini" file (in Saved/Config on the PC), under a [GrappleRunner] heading,
//  so they're still there next time you start the game.
//
//  THE SETTINGS
//    MouseSensitivity  how fast the view turns with the mouse (desktop)
//    GasDifficulty     0 = relaxed (gas lasts longer), 1 = normal, 2 = hard
//    PartnerChatter    true = the AI partner talks (subtitles in the world)
//
//  HOW TO READ THIS FILE
//  "inline" functions live in the header so any file can use them by including it.
//  GConfig is Unreal's reader/writer for .ini files.
// =====================================================================================
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

namespace GRSettings
{
	static const TCHAR* Section = TEXT("GrappleRunner");

	inline float GetFloat(const TCHAR* Key, float Default)
	{
		float Value = Default;
		if (GConfig)
		{
			GConfig->GetFloat(Section, Key, Value, GGameUserSettingsIni);
		}
		return Value;
	}

	inline void SetFloat(const TCHAR* Key, float Value)
	{
		if (GConfig)
		{
			GConfig->SetFloat(Section, Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);   // write it to disk now
		}
	}

	inline float MouseSensitivity() { return GetFloat(TEXT("MouseSensitivity"), 0.6f); }
	inline int32 GasDifficulty() { return FMath::Clamp(FMath::RoundToInt(GetFloat(TEXT("GasDifficulty"), 1.f)), 0, 2); }
	inline bool PartnerChatter() { return GetFloat(TEXT("PartnerChatter"), 1.f) > 0.5f; }

	// How fast gas burns at each difficulty: relaxed burns at 60%, hard at 140%.
	inline float GasDrainScale()
	{
		const int32 D = GasDifficulty();
		return D == 0 ? 0.6f : (D == 2 ? 1.4f : 1.f);
	}
	inline FString GasDifficultyName()
	{
		const int32 D = GasDifficulty();
		return D == 0 ? TEXT("RELAXED") : (D == 2 ? TEXT("HARD") : TEXT("NORMAL"));
	}
}

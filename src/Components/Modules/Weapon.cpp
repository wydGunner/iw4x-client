#include "Weapon.hpp"

#include "GSC/Script.hpp"
#include "Events.hpp"

namespace Components
{
	const Game::dvar_t* Weapon::BGWeaponOffHandFix;
	const Game::dvar_t* Weapon::CGRecoilMultiplier;
	const Game::dvar_t* Weapon::BGDisableDoubleTaps;

	Game::WeaponCompleteDef* Weapon::LoadWeaponCompleteDef(const char* name)
	{
		if (auto* rawWeaponFile = Game::BG_LoadWeaponCompleteDefInternal("mp", name))
		{
			// Fix for rumbles being wrong in the raw files.. This should not happen normally
			// But in effect, it happens very often, so we need to fix it to avoid rumble errors
			{
				if (rawWeaponFile->weapDef->notetrackRumbleMapKeys &&
					rawWeaponFile->weapDef->notetrackRumbleMapKeys[0] == rawWeaponFile->weapDef->notetrackSoundMapKeys[0])
				{
					// This means it's wrong and the dump is bad (it gave sound names as rumble names, which is not possible)
					// The zone has it right so let's take it from there
					const auto zoneWeapon = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_WEAPON, name).weapon;

					if (zoneWeapon)
					{
						// Restore rumble from zone
						std::memcpy(rawWeaponFile->weapDef->notetrackRumbleMapKeys, zoneWeapon->weapDef->notetrackRumbleMapKeys, 16 * sizeof(unsigned short));
						std::memcpy(rawWeaponFile->weapDef->notetrackRumbleMapValues, zoneWeapon->weapDef->notetrackRumbleMapValues, 16 * sizeof(unsigned short));

						rawWeaponFile->weapDef->fireRumble = zoneWeapon->weapDef->fireRumble;
						rawWeaponFile->weapDef->meleeImpactRumble = zoneWeapon->weapDef->meleeImpactRumble;
						rawWeaponFile->weapDef->turretBarrelSpinRumble = zoneWeapon->weapDef->turretBarrelSpinRumble;
					}


					// Oh well. You'll have to fix your weapon file :)
#define VALIDATE(weap, rumble)\
	if (weap->weapDef->rumble && *weap->weapDef->rumble)\
	{\
		std::string path = std::format("rumble/{}", weap->weapDef->rumble);\
		const auto rawfile = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_RAWFILE, path.c_str());\
		if (rawfile.data == nullptr)\
		{\
			Logger::Warning(Game::CON_CHANNEL_FILES, "Weapon {} has INVALID RUMBLE {} '{}'! Remove or replace it.\n", weap->szInternalName, #rumble, weap->weapDef->rumble);\
			weap->weapDef->rumble = nullptr;\
		}\
	}
					VALIDATE(rawWeaponFile, fireRumble);
					VALIDATE(rawWeaponFile, meleeImpactRumble);
					VALIDATE(rawWeaponFile, turretBarrelSpinRumble);

					for (auto i = 0; i < 16; ++i)
					{
						if (!rawWeaponFile->weapDef->notetrackRumbleMapKeys[i])
							break;

						auto noteTrackRumbleMap = rawWeaponFile->weapDef->notetrackRumbleMapValues;
						if (noteTrackRumbleMap[i])
						{
							auto str = Game::SL_ConvertToString(noteTrackRumbleMap[i]);
							if (str && *str)
							{
								std::string path = std::format("rumble/{}", str);
								const auto rawfile = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_RAWFILE, path.c_str());
								if (rawfile.data == nullptr || !strnlen(rawfile.rawfile->buffer, 1))
								{
									// Void it and warn the user
									noteTrackRumbleMap[i] = 0;
									rawWeaponFile->weapDef->notetrackRumbleMapKeys[i] = 0;

									Logger::Warning(
										Game::CON_CHANNEL_FILES,
										"Weapon {} has INVALID RUMBLE {} notetrackRumbleMap #'{}'! Remove or replace it.\n",
										rawWeaponFile->szInternalName,
										str,
										i
									);
								}
							}
						}
					}
#undef VALIDATE
				}
			}

			return rawWeaponFile;
		}

		auto* zoneWeaponFile = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_WEAPON, name).weapon;
		return Game::DB_IsXAssetDefault(Game::ASSET_TYPE_WEAPON, name) ? nullptr : zoneWeaponFile;
	}

	void Weapon::SaveRegisteredWeapons()
	{
		*reinterpret_cast<DWORD*>(0x1A86098) = 0;

		if (Game::BG_GetNumWeapons() > 1u)
		{
			for (unsigned int i = 1; i < Game::BG_GetNumWeapons(); ++i)
			{
				Game::SV_SetConfigstring(i + (i >= BASEGAME_WEAPON_LIMIT ? 2939 : 2804), Game::BG_GetWeaponName(i));
			}
		}
	}

	void Weapon::PatchHintStrings()
	{
		static Game::Material* hintMaterials[WEAPON_LIMIT + 4];

		// CG_RegisterWeapon
		Utils::Hook::Set(0x4EF619, &hintMaterials[4]);

		Utils::Hook::Set(0x58D003, hintMaterials); // CG_DrawCursorhint (All hints related to crosshair, like pickup hints)
		Utils::Hook::Set(0x58969A, &hintMaterials[3]); // hint_health in CG_RegisterGraphics
		Utils::Hook::Set(0x5896AB, &hintMaterials[4]); // hint_friendly in CG_RegisterGraphics

		Utils::Hook::Set<uint32_t>(0x58D01C + 1, ARRAYSIZE(hintMaterials));

		// Has to do with fx, but somehow lies within the material array
		// EDIT: Those are unrelated (PlayFX OnTag, with FX Id, which is also in cg_media but not related to hint materials)
		//Utils::Hook::Set(0x402069, &unknownMaterialArray[32]);
		//Utils::Hook::Set(0x4E05D9, &unknownMaterialArray[32]);

	}

	void Weapon::PatchLimit()
	{
		Utils::Hook::Set<DWORD>(0x403783, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x403E8C, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x41BC34, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x42EB42, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x44FA7B, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x474E0D, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x48E8F2, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x492647, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x494585, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x4945DB, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x4B1F96, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x4D4A99, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x4DD566, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x4E3683, WEAPON_LIMIT); // Configstring
		Utils::Hook::Set<DWORD>(0x58609F, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x586CAE, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x58F7BE, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x58F7D9, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x58F82D, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5D6C8B, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5D6CF7, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5E24D5, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5E2604, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5E2828, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5E2B4F, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5E366C, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5F2614, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5F7187, WEAPON_LIMIT);
		Utils::Hook::Set<DWORD>(0x5FECF9, WEAPON_LIMIT);

		Utils::Hook::Set<int>(0x586CC3, -(WEAPON_LIMIT));

		// Reference: https://courses.engr.illinois.edu/ece390/books/artofasm/CH09/CH09-6.html (See 9.5.2 Division Without DIV and IDIV)
		// And http://reverseengineering.stackexchange.com/questions/1397/how-can-i-reverse-optimized-integer-division-modulo-by-constant-operations
		// The game's magic number is computed using this formula: (1 / 1200) * (2 ^ (32 + 7)
		// I'm too lazy to generate the new magic number, so we can make use of the fact that using powers of 2 as scales allows to change the compensating shift
		static_assert(((WEAPON_LIMIT / BASEGAME_WEAPON_LIMIT) * BASEGAME_WEAPON_LIMIT) == WEAPON_LIMIT && (WEAPON_LIMIT / BASEGAME_WEAPON_LIMIT) != 0 && !((WEAPON_LIMIT / BASEGAME_WEAPON_LIMIT) & ((WEAPON_LIMIT / BASEGAME_WEAPON_LIMIT) - 1)), "WEAPON_LIMIT / 1200 is not a power of 2!");
		const unsigned char compensation = 7 + static_cast<unsigned char>(log2(WEAPON_LIMIT / BASEGAME_WEAPON_LIMIT)); // 7 is the compensation the game uses
		Utils::Hook::Set<BYTE>(0x49263D, compensation);
		Utils::Hook::Set<BYTE>(0x5E250C, compensation);
		Utils::Hook::Set<BYTE>(0x5E2B43, compensation);
		Utils::Hook::Set<BYTE>(0x5E352F, compensation);
		Utils::Hook::Set<BYTE>(0x5FECEF, compensation);

		static int bg_weaponCompleteDefs[WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4B35E1, sizeof(bg_weaponCompleteDefs));
		Utils::Hook::Set(0x44CE07, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x45C671, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x45C6A4, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x4B35E8, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x4B3643, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x4CE903, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x4CE927, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x4E6EC7, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57B69A, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57B910, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57B925, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57BA22, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57BA51, bg_weaponCompleteDefs);
		Utils::Hook::Set(0x57BA78, bg_weaponCompleteDefs);

		static int bg_weaponDefs[WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4B35F2, sizeof(bg_weaponDefs));
		Utils::Hook::Set(0x42F697, bg_weaponDefs);
		Utils::Hook::Set(0x440EB7, bg_weaponDefs);
		Utils::Hook::Set(0x45C67D, bg_weaponDefs);
		Utils::Hook::Set(0x45C685, bg_weaponDefs);
		Utils::Hook::Set(0x45C6B0, bg_weaponDefs);
		Utils::Hook::Set(0x4B35F9, bg_weaponDefs);
		Utils::Hook::Set(0x4B364B, bg_weaponDefs);
		Utils::Hook::Set(0x57B693, bg_weaponDefs);
		Utils::Hook::Set(0x57B75D, bg_weaponDefs);
		Utils::Hook::Set(0x57B809, bg_weaponDefs);
		Utils::Hook::Set(0x57B8D3, bg_weaponDefs);
		Utils::Hook::Set(0x57B96D, bg_weaponDefs);
		Utils::Hook::Set(0x57BA32, bg_weaponDefs);

		static int bg_weapClips[WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4B3603, sizeof(bg_weapClips));
		Utils::Hook::Set(0x4B360A, bg_weapClips);
		Utils::Hook::Set(0x4B3666, bg_weapClips);
		Utils::Hook::Set(0x57B993, bg_weapClips);
		Utils::Hook::Set(0x57B9E1, bg_weapClips);
		Utils::Hook::Set(0x57B9EA, bg_weapClips);

		static int bg_sharedAmmoCaps[WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4B3614, sizeof(bg_sharedAmmoCaps));
		Utils::Hook::Set(0x414D27, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x4B361B, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x4B365B, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B83B, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B870, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B87D, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B89C, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B8E0, bg_sharedAmmoCaps);
		Utils::Hook::Set(0x57B901, bg_sharedAmmoCaps);

		static int bg_weapAmmoTypes[WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4B3625, sizeof(bg_weapAmmoTypes));
		Utils::Hook::Set(0x4B362C, bg_weapAmmoTypes);
		Utils::Hook::Set(0x4B3650, bg_weapAmmoTypes);
		Utils::Hook::Set(0x57B783, bg_weapAmmoTypes);
		Utils::Hook::Set(0x57B7D1, bg_weapAmmoTypes);
		Utils::Hook::Set(0x57B7DA, bg_weapAmmoTypes);

		static int weaponStrings[WEAPON_LIMIT * 2]; // string + hash
		Utils::Hook::Set<DWORD>(0x504E01, sizeof(weaponStrings));
		Utils::Hook::Set<DWORD>(0x4C77DC, sizeof(weaponStrings));
		Utils::Hook::Set(0x4B72DC, weaponStrings);
		Utils::Hook::Set(0x4C77E2, weaponStrings);
		Utils::Hook::Set(0x504E08, weaponStrings);
		Utils::Hook::Set(0x795584, weaponStrings);
		Utils::Hook::Set(0x7955FC, weaponStrings);
		Utils::Hook::Set(0x4B72E8, &weaponStrings[1]);

		static char cg_weaponsArray[32 * WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4E3300, sizeof(cg_weaponsArray));
		Utils::Hook::Set<DWORD>(0x4E3305, sizeof(cg_weaponsArray));
		Utils::Hook::Set<DWORD>(0x4F1927, sizeof(cg_weaponsArray));
		Utils::Hook::Set<DWORD>(0x4F192C, sizeof(cg_weaponsArray));
		Utils::Hook::Set(0x4172D8, cg_weaponsArray);
		Utils::Hook::Set(0x45C96F, cg_weaponsArray);
		Utils::Hook::Set(0x45C9C7, cg_weaponsArray);
		Utils::Hook::Set(0x47F417, cg_weaponsArray);
		Utils::Hook::Set(0x4BA571, cg_weaponsArray);
		Utils::Hook::Set(0x4E330B, cg_weaponsArray);
		Utils::Hook::Set(0x4EF56C, cg_weaponsArray);
		Utils::Hook::Set(0x4F1934, cg_weaponsArray);
		Utils::Hook::Set(0x58B879, cg_weaponsArray);
		Utils::Hook::Set(0x58B98A, cg_weaponsArray);
		Utils::Hook::Set(0x58CCF7, cg_weaponsArray);
		Utils::Hook::Set(0x59BF05, cg_weaponsArray);
		Utils::Hook::Set(0x59C39B, cg_weaponsArray);
		Utils::Hook::Set(0x446D30, &cg_weaponsArray[8]);
		Utils::Hook::Set(0x59BD68, &cg_weaponsArray[13]);
		Utils::Hook::Set(0x58D0AE, &cg_weaponsArray[20]);

		static int cg_weaponsStaticArray[3 * WEAPON_LIMIT];
		Utils::Hook::Set<DWORD>(0x4E3322, sizeof(cg_weaponsStaticArray));
		Utils::Hook::Set<DWORD>(0x4F1912, sizeof(cg_weaponsStaticArray));
		Utils::Hook::Set(0x4548DE, cg_weaponsStaticArray);
		Utils::Hook::Set(0x4E3328, cg_weaponsStaticArray);
		Utils::Hook::Set(0x4EF57A, cg_weaponsStaticArray);
		Utils::Hook::Set(0x4F1919, cg_weaponsStaticArray);
		Utils::Hook::Set(0x59C095, cg_weaponsStaticArray);
		Utils::Hook::Set(0x59C09D, cg_weaponsStaticArray);

		PatchHintStrings();

		// Patch bg_weaponDefs on the stack
		Utils::Hook::Set<DWORD>(0x40C31D, sizeof(bg_weaponDefs));
		Utils::Hook::Set<DWORD>(0x40C32F, sizeof(bg_weaponDefs));
		Utils::Hook::Set<DWORD>(0x40C311, 0x258C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C45F, 0x258C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C478, 0x258C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C434, 0x258C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C434, 0x258C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		// Move second buffer pointers
		Utils::Hook::Set<DWORD>(0x40C336, 0x12E4 + ((sizeof(bg_weaponDefs)) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x40C3C6, 0x12DC + ((sizeof(bg_weaponDefs)) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x40C3CE, 0x12DC + ((sizeof(bg_weaponDefs)) - (BASEGAME_WEAPON_LIMIT * 4)));
		// Move arg0 pointers
		Utils::Hook::Set<DWORD>(0x40C365, 0x259C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C44E, 0x259C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		Utils::Hook::Set<DWORD>(0x40C467, 0x259C + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));
		// Move arg4 pointers
		Utils::Hook::Set<DWORD>(0x40C344, 0x25B4 + ((sizeof(bg_weaponDefs) * 2) - (BASEGAME_WEAPON_LIMIT * 4 * 2)));

		// Patch bg_sharedAmmoCaps on the stack
		Utils::Hook::Set<DWORD>(0x4F76E6, sizeof(bg_sharedAmmoCaps));
		Utils::Hook::Set<DWORD>(0x4F7621, 0x12C8 + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x4F76AF, 0x12C8 + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x4F76DA, 0x12C8 + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x4F77C5, 0x12C8 + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		// Move arg0 pointers
		Utils::Hook::Set<DWORD>(0x4F766D, 0x12DC + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x4F76B7, 0x12DC + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		Utils::Hook::Set<DWORD>(0x4F76FB, 0x12EC + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
		// Move arg4 pointers
		Utils::Hook::Set<DWORD>(0x4F7630, 0x12DC + (sizeof(bg_sharedAmmoCaps) - (BASEGAME_WEAPON_LIMIT * 4)));
	}

	void* Weapon::LoadNoneWeaponHook()
	{
		// load anim scripts now, rather than a bit later on
		Utils::Hook::Call<void()>(0x4E46A0)();

		return Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_WEAPON, "none").data;
	}

	void __declspec(naked) Weapon::LoadNoneWeaponHookStub()
	{
		__asm
		{
			jmp LoadNoneWeaponHook
		}
	}

	void __declspec(naked) Weapon::CG_UpdatePrimaryForAltModeWeapon_Stub()
	{
		__asm
		{
			mov eax, 0x440EB0 // BG_GetWeaponDef
			call eax
			add esp, 0x4

			test eax, eax
			jz null

			// Game code
			push 0x59E349
			retn

			null :
			mov al, 1
				ret
		}
	}

	void __declspec(naked) Weapon::CG_SelectWeaponIndex_Stub()
	{
		__asm
		{
			mov eax, 0x440EB0 // BG_GetWeaponDef
			call eax
			add esp, 0x4

			test eax, eax
			jz null

			// Game code
			push 0x48BB2D
			ret

			null :
			push 0x48BB1F // Exit function
				ret
		}
	}

	void __declspec(naked) Weapon::WeaponEntCanBeGrabbed_Stub()
	{
		using namespace Game;

		__asm
		{
			cmp dword ptr[esp + 0x8], 0x0
			jz touched

			push 0x56E82C
			ret

			touched :
			test dword ptr[edi + 0x2BC], PWF_DISABLE_WEAPON_PICKUP
				jnz exit_func

				// Game code
				test eax, eax
				jz continue_func

				exit_func :
			xor eax, eax
				ret

				continue_func :
			push 0x56E84C
				ret
		}
	}

	__declspec(naked) void Weapon::JavelinResetHook_Stub()
	{
		static DWORD PM_Weapon_OffHandEnd_t = 0x577A10;

		__asm
		{
			call PM_Weapon_OffHandEnd_t

			push eax
			mov eax, BGWeaponOffHandFix
			cmp byte ptr[eax + 0x10], 1
			pop eax

			jne safeReturn

			mov dword ptr[esi + 0x34], 0 // playerState_s.grenadeTimeLeft

			safeReturn:
			pop edi
				pop esi
				pop ebx
				ret
		}
	}

	void Weapon::BG_WeaponFireRecoil_Stub(
		void* ps,
		float* recoilSpeed,
		float* kickAVel,
		unsigned int* holdrand,
		Game::PlayerHandIndex hand
	)
	{
		float adjustedRecoilSpeed[3]{};
		float adjustedKick[3]{};


		Utils::Hook::Call<void(void*, float*, float*, unsigned int*, Game::PlayerHandIndex)>(0x4A5FE0)(
			ps,
			adjustedRecoilSpeed,
			adjustedKick,
			holdrand,
			hand
		);

		for (size_t axis = 0; axis < 3; axis++)
		{
			recoilSpeed [axis] = adjustedRecoilSpeed[axis] * CGRecoilMultiplier->current.value;
			kickAVel [axis] = adjustedKick[axis] * CGRecoilMultiplier->current.value;
		}
	}

	void Weapon::PlayerCmd_InitialWeaponRaise(const Game::scr_entref_t entref)
	{
		auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
		const auto* weapon = Game::Scr_GetString(0);
		const auto index = Game::G_GetWeaponIndexForName(weapon);

		auto* ps = &ent->client->ps;
		if (!Game::BG_IsWeaponValid(ps, index))
		{
			Game::Scr_Error(Utils::String::VA("invalid InitialWeaponRaise: %s", weapon));
			return;
		}

		assert(ps);

		if (!index)
		{
			return;
		}

		auto* equippedWeapon = Game::BG_GetEquippedWeaponState(ps, index);
		if (!equippedWeapon)
		{
			return;
		}

		equippedWeapon->usedBefore = false;
		Game::Player_SwitchToWeapon(ent);
	}

	void Weapon::PlayerCmd_FreezeControlsAllowLook(const Game::scr_entref_t entref)
	{
		const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

		if (Game::Scr_GetInt(0))
		{
			ent->client->ps.weapCommon.weapFlags |= Game::PWF_DISABLE_WEAPONS;
			ent->client->flags |= Game::CF_BIT_DISABLE_USABILITY;
		}
		else
		{
			ent->client->ps.weapCommon.weapFlags &= ~Game::PWF_DISABLE_WEAPONS;
			ent->client->flags &= ~Game::CF_BIT_DISABLE_USABILITY;
		}
	}

	void Weapon::AddScriptMethods()
	{
		GSC::Script::AddMethod("DisableWeaponPickup", [](const Game::scr_entref_t entref)
			{
				const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

				ent->client->ps.weapCommon.weapFlags |= Game::PWF_DISABLE_WEAPON_PICKUP;
			});

		GSC::Script::AddMethod("EnableWeaponPickup", [](const Game::scr_entref_t entref)
			{
				const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

				ent->client->ps.weapCommon.weapFlags &= ~Game::PWF_DISABLE_WEAPON_PICKUP;
			});

		// PlayerCmd_AreControlsFrozen GSC function from Black Ops 2
		GSC::Script::AddMethod("AreControlsFrozen", [](Game::scr_entref_t entref) // Usage: self AreControlsFrozen();
			{
				const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				Game::Scr_AddBool((ent->client->flags & Game::CF_BIT_FROZEN) != 0);
			});

		// ---------------------------------------------------------------------
		// Trickshot weapon-state methods, restored to match the behaviour of the
		// older custom IW4x build (reverse-engineered from its handlers).
		// Both hands (weapState[0] = right, weapState[1] = left) are written,
		// exactly as the old build did. Extra arguments are accepted and ignored
		// so existing scripts like `self setWeaponAnim(24, 2);` keep working.
		// ---------------------------------------------------------------------

		// self setWeaponAnim(<anim>);
		GSC::Script::AddMethod("SetWeaponAnim", [](const Game::scr_entref_t entref)
			{
				auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				const auto anim = Game::Scr_GetInt(0);
				ent->client->ps.weapState[Game::WEAPON_HAND_RIGHT].weapAnim = anim;
				ent->client->ps.weapState[Game::WEAPON_HAND_LEFT].weapAnim = anim;
			});

		// self setWeaponAnimTime(<ms>);
		GSC::Script::AddMethod("SetWeaponAnimTime", [](const Game::scr_entref_t entref)
			{
				auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				const auto time = Game::Scr_GetInt(0);
				ent->client->ps.weapState[Game::WEAPON_HAND_RIGHT].weaponTime = time;
				ent->client->ps.weapState[Game::WEAPON_HAND_LEFT].weaponTime = time;
			});

		// self setWeaponIdleTime(<ms>);
		GSC::Script::AddMethod("SetWeaponIdleTime", [](const Game::scr_entref_t entref)
			{
				auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				ent->client->ps.weapCommon.weaponIdleTime = Game::Scr_GetInt(0);
			});

		// anim = self getWeaponAnim();
		GSC::Script::AddMethod("GetWeaponAnim", [](const Game::scr_entref_t entref)
			{
				const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				Game::Scr_AddInt(ent->client->ps.weapState[Game::WEAPON_HAND_RIGHT].weapAnim);
			});

		// self instashoot();  - weapon ready immediately, no delay/timer
		GSC::Script::AddMethod("Instashoot", [](const Game::scr_entref_t entref)
			{
				auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
				for (auto& ws : ent->client->ps.weapState)
				{
					ws.weaponState = Game::WEAPON_READY;
					ws.weaponTime = 0;
					ws.weaponDelay = 0;
				}
			});

		// ---------------------------------------------------------------------
		// weapdef_* functions, restored from the same older custom build.
		// They edit the loaded weapon definition for EVERY player using that
		// weapon until the map restarts. Usage: weapdef_<x>(<weaponName>, <value>)
		// ---------------------------------------------------------------------
		static const auto getWeaponDefArg = []() -> Game::WeaponDef*
		{
			const auto* name = Game::Scr_GetString(0);
			const auto index = Game::G_GetWeaponIndexForName(name);
			if (!index)
			{
				// Print instead of Scr_Error so a typo doesn't kill the calling script thread
				Logger::Print("weapdef: unknown weapon '{}'\n", name ? name : "");
				return nullptr;
			}
			return Game::BG_GetWeaponDef(index);
		};

		// weapdef_forcecowboy(<weapon>, <maxPitch>)  - fGunMaxPitch
		GSC::Script::AddFunction("weapdef_forcecowboy", []
			{
				if (auto* def = getWeaponDefArg()) def->fGunMaxPitch = Game::Scr_GetFloat(1);
			});

		// weapdef_quickdroptime(<weapon>, <ms>)
		GSC::Script::AddFunction("weapdef_quickdroptime", []
			{
				if (auto* def = getWeaponDefArg()) def->quickDropTime = Game::Scr_GetInt(1);
			});

		// weapdef_droptime(<weapon>, <ms>)
		GSC::Script::AddFunction("weapdef_droptime", []
			{
				if (auto* def = getWeaponDefArg()) def->iDropTime = Game::Scr_GetInt(1);
			});

		// weapdef_holdfiretime(<weapon>, <ms>)
		GSC::Script::AddFunction("weapdef_holdfiretime", []
			{
				if (auto* def = getWeaponDefArg()) def->iHoldFireTime = Game::Scr_GetInt(1);
			});

		// weapdef_sprintintime(<weapon>, <ms>)
		GSC::Script::AddFunction("weapdef_sprintintime", []
			{
				if (auto* def = getWeaponDefArg()) def->sprintInTime = Game::Scr_GetInt(1);
			});

		// weapdef_reloadTime(<weapon>, <ms>)
		GSC::Script::AddFunction("weapdef_reloadTime", []
			{
				if (auto* def = getWeaponDefArg()) def->iReloadTime = Game::Scr_GetInt(1);
			});

		// weapdef_kickback(<weapon>, <pitch>)  - same value for hip & ADS min/max
		GSC::Script::AddFunction("weapdef_kickback", []
			{
				if (auto* def = getWeaponDefArg())
				{
					const auto v = Game::Scr_GetFloat(1);
					def->fHipGunKickPitchMax = v;
					def->fHipGunKickPitchMin = v;
					def->fAdsGunKickPitchMax = v;
					def->fAdsGunKickPitchMin = v;
				}
			});

		GSC::Script::AddMethod("InitialWeaponRaise", PlayerCmd_InitialWeaponRaise);
		GSC::Script::AddMethod("FreezeControlsAllowLook", PlayerCmd_FreezeControlsAllowLook);
	}

	// =====================================================================
	// Live weapon editing by weapon-file field name
	// ---------------------------------------------------------------------
	// WeapDefSet("ak47_mp", "reloadTime", 4.5) edits the loaded weapon the
	// same way the weapon file would have set it - same names, same units
	// (seconds for *Time fields), no restart. The first edit of a field
	// remembers its original value so it can be read back / reset.
	//
	// The field list is the game's own weapon-file parse table, so any
	// numeric field from a weapon file works (int, bool, float, seconds).
	// Anim fields (fireAnim, meleeAnim, raiseAnim...) swap that anim in the
	// weapon's viewmodel anim set, e.g. meleeAnim viewmodel_knife_melee_stab.
	//
	// Engine data (iw4x.exe, found by disassembly):
	//   0x795F00  cspField_t[0x2A0] weapon field table {name, offset, type}
	//             offsets are into the loader's flat "full def":
	//               0x000 WeaponCompleteDef (0x74 bytes)
	//               0x074 WeaponDef         (0x684 bytes)
	//               0x738 szXAnims[37]      (anim name per weapAnimFiles_t)
	//   0x4EF57A  operand of CG_RegisterWeapon's "lea edi,[edi*4+anims]"
	//             -> per-weapon viewmodel XAnim_s* x3 (default/right/left).
	//             IW4x relocates that array, so read the address from here.
	//   0x4EF790  XAnimCreate(XAnim_s*, animIndex, name)
	//             XAnim_s = { uint count; { u16 n; u16 pad; XAnimParts* parts } [count] }
	//
	// Notes
	//  - WeaponDef fields (reload/rechamber/raise/drop/sprint times...) can
	//    be shared by all attachment variants of a gun in the fastfiles, so
	//    editing ak47_fmj_mp may also change ak47_mp. CompleteDef fields
	//    (fireTime, firstRaiseTime, adsZoomFov) and anims are per variant.
	//  - Edits live in this process: on a listen server they change both the
	//    host's server and client. Other players' clients keep stock values.
	//  - Stock (fastfile) weapons stay loaded for the whole game session, so
	//    edits survive fast_restart and map changes. Every edit remembers
	//    the original AND the value it wrote: if the field still holds what
	//    we wrote, reset restores the original; if not, the game reloaded the
	//    weapon and the current value is stock. Nothing is ever forgotten.
	// =====================================================================
	namespace
	{
		constexpr std::uintptr_t WEAPON_FIELD_TABLE = 0x795F00;
		constexpr int WEAPON_FIELD_COUNT = 0x2A0;

		constexpr int FULLDEF_WEAPDEF = 0x74;
		constexpr int FULLDEF_WEAPDEF_END = 0x74 + 0x684;
		constexpr int FULLDEF_XANIMS = 0x738;

		enum
		{
			CSPFT_STRING = 0,
			CSPFT_INT = 4,
			CSPFT_BOOL = 6,
			CSPFT_FLOAT = 7,
			CSPFT_MILLISECONDS = 9,
		};

		constexpr std::uintptr_t CG_WEAPON_ANIMS_OPERAND = 0x4EF57A;
		constexpr int CG_WEAPON_ANIM_SETS = 3;

		using XAnimCreate_t = void(*)(void* anims, unsigned int animIndex, const char* name);
		const auto XAnimCreate = reinterpret_cast<XAnimCreate_t>(0x4EF790);

		// One entry per edited field address (shared WeaponDefs share it)
		struct SavedValue
		{
			std::uint32_t original;
			std::uint32_t written;
			int size;
		};

		// One entry per edited viewmodel anim entry (per weapon, per set)
		struct SavedAnim
		{
			std::uint32_t original[2];
			std::uint32_t written[2];
			std::string name;
		};

		std::unordered_map<std::uintptr_t, SavedValue> weapDefSaved;
		std::unordered_map<std::uintptr_t, SavedAnim> weapAnimSaved;

		const Game::cspField_t* FindWeaponField(const char* name)
		{
			if (!name || !*name) return nullptr;

			const auto* fields = reinterpret_cast<const Game::cspField_t*>(WEAPON_FIELD_TABLE);
			for (auto i = 0; i < WEAPON_FIELD_COUNT; ++i)
			{
				if (fields[i].szName && !_stricmp(fields[i].szName, name))
				{
					return &fields[i];
				}
			}

			return nullptr;
		}

		bool IsAnimField(const Game::cspField_t* field)
		{
			return field->iFieldType == CSPFT_STRING
				&& field->iOffset >= FULLDEF_XANIMS
				&& field->iOffset < FULLDEF_XANIMS + static_cast<int>(Game::NUM_WEAP_ANIMS) * 4;
		}

		unsigned int AnimIndexOf(const Game::cspField_t* field)
		{
			return static_cast<unsigned int>((field->iOffset - FULLDEF_XANIMS) / 4);
		}

		bool IsNumericField(const Game::cspField_t* field)
		{
			switch (field->iFieldType)
			{
			case CSPFT_INT:
			case CSPFT_BOOL:
			case CSPFT_FLOAT:
			case CSPFT_MILLISECONDS:
				return field->iOffset >= 0 && field->iOffset < FULLDEF_WEAPDEF_END;
			default:
				return false;
			}
		}

		int FieldSize(const Game::cspField_t* field)
		{
			return field->iFieldType == CSPFT_BOOL ? 1 : 4;
		}

		// Map a full-def offset onto the live, separately allocated structs
		std::uint8_t* GetFieldAddress(const unsigned int weapIndex, const Game::cspField_t* field)
		{
			auto* completeDef = Game::BG_GetWeaponCompleteDef(weapIndex);
			if (!completeDef) return nullptr;

			if (field->iOffset < FULLDEF_WEAPDEF)
			{
				return reinterpret_cast<std::uint8_t*>(completeDef) + field->iOffset;
			}

			if (field->iOffset < FULLDEF_WEAPDEF_END && completeDef->weapDef)
			{
				return reinterpret_cast<std::uint8_t*>(completeDef->weapDef) + (field->iOffset - FULLDEF_WEAPDEF);
			}

			return nullptr;
		}

		std::uint32_t ReadRaw(const std::uint8_t* address, const int size)
		{
			std::uint32_t value = 0;
			std::memcpy(&value, address, size);
			return value;
		}

		// Value in weapon-file units (seconds for ms fields)
		float ReadNumeric(const std::uint8_t* address, const int type)
		{
			switch (type)
			{
			case CSPFT_BOOL: return *address ? 1.0f : 0.0f;
			case CSPFT_FLOAT: return *reinterpret_cast<const float*>(address);
			case CSPFT_MILLISECONDS: return static_cast<float>(*reinterpret_cast<const int*>(address)) / 1000.0f;
			default: return static_cast<float>(*reinterpret_cast<const int*>(address));
			}
		}

		void WriteNumeric(std::uint8_t* address, const int type, const float value)
		{
			switch (type)
			{
			case CSPFT_BOOL: *address = value != 0.0f ? 1 : 0; break;
			case CSPFT_FLOAT: *reinterpret_cast<float*>(address) = value; break;
			case CSPFT_MILLISECONDS: *reinterpret_cast<int*>(address) = static_cast<int>(std::lround(value * 1000.0f)); break;
			default: *reinterpret_cast<int*>(address) = static_cast<int>(std::lround(value)); break;
			}
		}

		// Is our edit at this address still in place? (if not, the game
		// reloaded the weapon and whatever is there now is stock)
		const SavedValue* LiveEdit(const std::uint8_t* address)
		{
			const auto it = weapDefSaved.find(reinterpret_cast<std::uintptr_t>(address));
			if (it == weapDefSaved.end()) return nullptr;
			if (ReadRaw(address, it->second.size) != it->second.written) return nullptr;
			return &it->second;
		}

		void WriteField(std::uint8_t* address, const Game::cspField_t* field, const float value)
		{
			const auto size = FieldSize(field);
			const auto key = reinterpret_cast<std::uintptr_t>(address);

			// First edit, or the weapon was reloaded since: current value is stock
			if (!LiveEdit(address))
			{
				const auto current = ReadRaw(address, size);
				weapDefSaved[key] = SavedValue{ current, current, size };
			}

			WriteNumeric(address, field->iFieldType, value);
			weapDefSaved[key].written = ReadRaw(address, size);
		}

		void RestoreField(std::uint8_t* address)
		{
			const auto key = reinterpret_cast<std::uintptr_t>(address);
			if (const auto* saved = LiveEdit(address))
			{
				std::memcpy(address, &saved->original, saved->size);
			}
			weapDefSaved.erase(key);
		}

		std::uintptr_t* GetWeaponAnimSets(const unsigned int weapIndex)
		{
			const auto base = *reinterpret_cast<std::uintptr_t*>(CG_WEAPON_ANIMS_OPERAND);
			return reinterpret_cast<std::uintptr_t*>(base + weapIndex * CG_WEAPON_ANIM_SETS * sizeof(std::uintptr_t));
		}

		std::uint32_t* GetAnimEntry(const std::uintptr_t anims, const unsigned int animIndex)
		{
			if (!anims || animIndex >= *reinterpret_cast<unsigned int*>(anims)) return nullptr;
			return reinterpret_cast<std::uint32_t*>(anims + 4 + animIndex * 8);
		}

		const SavedAnim* LiveAnimEdit(const std::uint32_t* entry)
		{
			const auto it = weapAnimSaved.find(reinterpret_cast<std::uintptr_t>(entry));
			if (it == weapAnimSaved.end()) return nullptr;
			if (entry[0] != it->second.written[0] || entry[1] != it->second.written[1]) return nullptr;
			return &it->second;
		}

		void RestoreAnimEntry(std::uint32_t* entry)
		{
			if (const auto* saved = LiveAnimEdit(entry))
			{
				entry[0] = saved->original[0];
				entry[1] = saved->original[1];
			}
			weapAnimSaved.erase(reinterpret_cast<std::uintptr_t>(entry));
		}

		// name == nullptr: restore the original anim
		bool SwapWeaponAnim(const unsigned int weapIndex, const unsigned int animIndex, const char* name)
		{
			if (name && Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XANIMPARTS, name))
			{
				Logger::Print("weapdef: anim '{}' is not loaded\n", name);
				return false;
			}

			auto* sets = GetWeaponAnimSets(weapIndex);
			auto found = false;

			for (auto set = 0; set < CG_WEAPON_ANIM_SETS; ++set)
			{
				auto* entry = GetAnimEntry(sets[set], animIndex);
				if (!entry) continue;
				found = true;

				if (!name)
				{
					RestoreAnimEntry(entry);
					continue;
				}

				const auto key = reinterpret_cast<std::uintptr_t>(entry);
				if (!LiveAnimEdit(entry))
				{
					weapAnimSaved[key] = SavedAnim{ { entry[0], entry[1] }, { entry[0], entry[1] }, {} };
				}

				XAnimCreate(reinterpret_cast<void*>(sets[set]), animIndex, name);

				auto& saved = weapAnimSaved[key];
				saved.written[0] = entry[0];
				saved.written[1] = entry[1];
				saved.name = name;
			}

			return found || !name;
		}

		const char* GetOriginalAnimName(const unsigned int weapIndex, const unsigned int animIndex)
		{
			const auto* completeDef = Game::BG_GetWeaponCompleteDef(weapIndex);
			if (!completeDef || !completeDef->szXAnims || !completeDef->szXAnims[animIndex]) return "";
			return completeDef->szXAnims[animIndex];
		}

		const char* GetCurrentAnimName(const unsigned int weapIndex, const unsigned int animIndex)
		{
			auto* entry = GetAnimEntry(GetWeaponAnimSets(weapIndex)[0], animIndex);
			if (entry)
			{
				if (const auto* saved = LiveAnimEdit(entry)) return saved->name.c_str();
			}
			return GetOriginalAnimName(weapIndex, animIndex);
		}

		unsigned int GetWeaponArg(const unsigned int index)
		{
			const auto* name = Game::Scr_GetString(index);
			const auto weapIndex = Game::G_GetWeaponIndexForName(name);
			if (!weapIndex)
			{
				Logger::Print("weapdef: unknown weapon '{}'\n", name ? name : "");
			}
			return weapIndex;
		}

		const Game::cspField_t* GetFieldArg(const unsigned int index)
		{
			const auto* name = Game::Scr_GetString(index);
			const auto* field = FindWeaponField(name);
			if (!field || (!IsNumericField(field) && !IsAnimField(field)))
			{
				Logger::Print("weapdef: field '{}' is unknown or not editable live\n", name ? name : "");
				return nullptr;
			}
			return field;
		}
	}

	bool Weapon::WeapDef_Set(const unsigned int weapIndex, const char* fieldName, const char* value)
	{
		const auto* field = FindWeaponField(fieldName);
		if (!weapIndex || !field || !value) return false;

		if (IsAnimField(field))
		{
			const auto restore = !*value || !_stricmp(value, "default");
			return SwapWeaponAnim(weapIndex, AnimIndexOf(field), restore ? nullptr : value);
		}

		if (!IsNumericField(field)) return false;

		auto* address = GetFieldAddress(weapIndex, field);
		if (!address) return false;

		WriteField(address, field, std::strtof(value, nullptr));
		return true;
	}

	void Weapon::WeapDef_Reset(const unsigned int weapIndex, const char* fieldName)
	{
		if (fieldName)
		{
			const auto* field = FindWeaponField(fieldName);
			if (!field) return;

			if (IsAnimField(field))
			{
				SwapWeaponAnim(weapIndex, AnimIndexOf(field), nullptr);
			}
			else if (IsNumericField(field))
			{
				if (auto* address = GetFieldAddress(weapIndex, field)) RestoreField(address);
			}
			return;
		}

		// Whole weapon: every numeric field + every anim
		const auto* fields = reinterpret_cast<const Game::cspField_t*>(WEAPON_FIELD_TABLE);
		for (auto i = 0; i < WEAPON_FIELD_COUNT; ++i)
		{
			if (!IsNumericField(&fields[i])) continue;
			if (auto* address = GetFieldAddress(weapIndex, &fields[i])) RestoreField(address);
		}

		for (auto anim = 0u; anim < static_cast<unsigned int>(Game::NUM_WEAP_ANIMS); ++anim)
		{
			SwapWeaponAnim(weapIndex, anim, nullptr);
		}
	}

	void Weapon::WeapDef_ResetAll()
	{
		for (auto& [key, saved] : weapDefSaved)
		{
			auto* address = reinterpret_cast<std::uint8_t*>(key);
			if (ReadRaw(address, saved.size) == saved.written)
			{
				std::memcpy(address, &saved.original, saved.size);
			}
		}
		weapDefSaved.clear();

		for (auto& [key, saved] : weapAnimSaved)
		{
			auto* entry = reinterpret_cast<std::uint32_t*>(key);
			if (entry[0] == saved.written[0] && entry[1] == saved.written[1])
			{
				entry[0] = saved.original[0];
				entry[1] = saved.original[1];
			}
		}
		weapAnimSaved.clear();
	}

	void Weapon::AddWeapDefEditing()
	{
		AssertSize(Game::WeaponCompleteDef, FULLDEF_WEAPDEF);
		AssertSize(Game::WeaponDef, FULLDEF_WEAPDEF_END - FULLDEF_WEAPDEF);

		// WeapDefSet(<weapon>, <field>, <value>)
		//   numeric fields: number, seconds for *Time fields
		//   anim fields:    anim name, or "" / "default" to restore
		GSC::Script::AddFunction("WeapDefSet", []
			{
				const auto weapIndex = GetWeaponArg(0);
				const auto* field = GetFieldArg(1);
				if (!weapIndex || !field) return;

				if (IsAnimField(field))
				{
					WeapDef_Set(weapIndex, field->szName, Game::Scr_GetString(2));
				}
				else
				{
					WeapDef_Set(weapIndex, field->szName, std::to_string(Game::Scr_GetFloat(2)).c_str());
				}
			});

		// value = WeapDefGet(<weapon>, <field>, [original = 0])
		GSC::Script::AddFunction("WeapDefGet", []
			{
				const auto weapIndex = GetWeaponArg(0);
				const auto* field = GetFieldArg(1);
				const auto original = Game::Scr_GetNumParam() > 2 && Game::Scr_GetInt(2) != 0;
				if (!weapIndex || !field)
				{
					Game::Scr_AddInt(0);
					return;
				}

				if (IsAnimField(field))
				{
					const auto animIndex = AnimIndexOf(field);
					Game::Scr_AddString(original ? GetOriginalAnimName(weapIndex, animIndex) : GetCurrentAnimName(weapIndex, animIndex));
					return;
				}

				auto* address = GetFieldAddress(weapIndex, field);
				if (!address)
				{
					Game::Scr_AddInt(0);
					return;
				}

				if (original)
				{
					if (const auto* saved = LiveEdit(address))
					{
						Game::Scr_AddFloat(ReadNumeric(reinterpret_cast<const std::uint8_t*>(&saved->original), field->iFieldType));
						return;
					}
				}

				Game::Scr_AddFloat(ReadNumeric(address, field->iFieldType));
			});

		// WeapDefReset(<weapon>, [field])   no field = everything on that weapon
		GSC::Script::AddFunction("WeapDefReset", []
			{
				const auto weapIndex = GetWeaponArg(0);
				if (!weapIndex) return;
				WeapDef_Reset(weapIndex, Game::Scr_GetNumParam() > 1 ? Game::Scr_GetString(1) : nullptr);
			});

		// WeapDefResetAll()   every edited weapon back to stock
		GSC::Script::AddFunction("WeapDefResetAll", []
			{
				WeapDef_ResetAll();
			});

		// Console: weapdef <weapon> <field> <value>   /   weapdef_reset <weapon> [field]   /   weapdef_resetall
		Command::Add("weapdef", [](const Command::Params* params)
			{
				if (params->size() < 4)
				{
					Logger::Print("usage: weapdef <weapon> <field> <value>   e.g. weapdef cheytac_mp reloadTime 4.534\n");
					return;
				}

				const auto weapIndex = Game::G_GetWeaponIndexForName(params->get(1));
				if (!weapIndex || !WeapDef_Set(weapIndex, params->get(2), params->get(3)))
				{
					Logger::Print("weapdef: could not set {} on {}\n", params->get(2), params->get(1));
				}
			});

		Command::Add("weapdef_reset", [](const Command::Params* params)
			{
				if (params->size() < 2)
				{
					Logger::Print("usage: weapdef_reset <weapon> [field]\n");
					return;
				}

				const auto weapIndex = Game::G_GetWeaponIndexForName(params->get(1));
				if (weapIndex) WeapDef_Reset(weapIndex, params->size() > 2 ? params->get(2) : nullptr);
			});

		Command::Add("weapdef_resetall", []
			{
				WeapDef_ResetAll();
				Logger::Print("weapdef: every edited weapon reset to stock\n");
			});
	}

	// =====================================================================
	// Viewmodel anim clip player (layered)
	// ---------------------------------------------------------------------
	// Up to ANIM_CLIP_LAYERS clips at once. Each layer plays one viewmodel
	// animation from <start>% to <end>% at <speed>x with a blend <weight>,
	// optionally freezing on the end frame. Layers blend together, e.g.
	// First Raise on layer 1 + Empty Reload on layer 2.
	//
	// animclip_keepads 1 (default): the two ADS anims (35/36) are left to
	// the game, so aiming down sights still works while a clip plays.
	// animclip_mixgame 1: the game's own anims (fire, reload...) are not
	// silenced, so shooting during a clip still shows the fire anim
	// blended with the clip. Default 0 = only the clip is visible.
	// (Firing itself always works - it is server-side and never touched.)
	//
	// Purely client-side: only the view of the local player running this
	// DLL changes - firing, timers and the server never see it.
	//
	// Engine addresses (iw4x.exe, found by disassembly):
	//   0x4D15C0  DObjGetTree(DObj*) -> XAnimTree_s*
	//   0x444B90  XAnimSetGoalWeight(DObj*, anim, weight, goalTime, rate,
	//                                notifyName, notifyType, restart)
	//   0x4DC640  XAnimSetTime(XAnimTree_s*, anim, time)   time = 0..1
	//   0x444AA0  XAnimGetTime(XAnimTree_s*, anim) -> float  0..1
	//   0x59B5E0  WeaponRunXModelAnims  eax = hand, push playerState
	//             (one call site: 0x47F55C, once per hand per frame)
	//   0x9F5D28  per-hand viewmodel info, stride 0x14:
	//             +0x0 DObj* (viewmodel), +0xC last weapAnim it started
	// The game's own PlayADSAnim (0x59B420) uses this same trick: goal
	// weight 1 at rate 0, then XAnimSetTime to scrub the ADS anims.
	// =====================================================================
	namespace
	{
		using DObjGetTree_t = Game::XAnimTree_s*(*)(Game::DObj* obj);
		using XAnimSetGoalWeight_t = int(*)(Game::DObj* obj, unsigned int animIndex, float goalWeight, float goalTime, float rate, unsigned int notifyName, unsigned int notifyType, int restart);
		using XAnimSetTime_t = void(*)(Game::XAnimTree_s* tree, unsigned int animIndex, float time);
		using XAnimGetTime_t = float(*)(Game::XAnimTree_s* tree, unsigned int animIndex);

		const auto DObjGetTree = reinterpret_cast<DObjGetTree_t>(0x4D15C0);
		const auto XAnimSetGoalWeight = reinterpret_cast<XAnimSetGoalWeight_t>(0x444B90);
		const auto XAnimSetTime = reinterpret_cast<XAnimSetTime_t>(0x4DC640);
		const auto XAnimGetTime = reinterpret_cast<XAnimGetTime_t>(0x444AA0);

		constexpr std::uintptr_t VIEWMODEL_HANDS = 0x9F5D28;
		constexpr std::uintptr_t VIEWMODEL_HAND_SIZE = 0x14;
		constexpr std::uintptr_t VIEWMODEL_HAND_PREV_ANIM = 0xC;

		// Anims 1..36 are children of the VIEWMODEL tree (0 is root)
		constexpr unsigned int CLIP_FIRST_ANIM = Game::WEAP_ANIM_IDLE;
		constexpr unsigned int CLIP_LAST_ANIM = Game::NUM_WEAP_ANIMS - 1;

		// Highest time we seek to; 1.0 wraps to 0 on looping anims
		constexpr float CLIP_MAX_TIME = 0.999f;

		constexpr int ANIM_CLIP_LAYERS = 4;

		// Playback modes (animclip arg 8)
		enum ClipMode
		{
			CLIP_FORWARD = 0,
			CLIP_REVERSE = 1,
			CLIP_PINGPONG = 2,       // start -> end -> start, once
			CLIP_LOOP = 3,           // start -> end, repeat until stopped
			CLIP_PINGPONG_LOOP = 4,  // bounce until stopped
			CLIP_MODE_COUNT
		};

		// Curves over the clip's progress (0..1). Speed curves average 1x so
		// the clip takes about as long as it does flat.
		enum ClipCurve
		{
			CURVE_FLAT = 0,
			CURVE_IN = 1,       // speed: slow -> fast   weight: fade in
			CURVE_OUT = 2,      // speed: fast -> slow   weight: fade out
			CURVE_IN_OUT = 3,   // speed: slow ends      weight: up then down
			CURVE_COUNT
		};

		struct AnimClipLayer
		{
			bool active;
			bool frozen;
			bool freezeAtEnd;
			unsigned int anim;
			float start;
			float end;
			float speed;
			float weight;
			int mode;
			int speedCurve;
			int weightCurve;
			int holdMs;      // freeze: how long to hold the end pose (0 = until stopped)
			int blendMs;     // fade in at the start, fade out when it ends

			// runtime (driven from the right hand, copied to the left)
			bool pendingStart;
			float pos;        // anim time we put it at
			float lastSet;    // what we set last frame (engine advances from it)
			int dir;          // +1 / -1
			bool bounced;     // ping-pong: on the way back
			int startedAt;
			int frozenAt;
			bool fading;
			int fadeAt;
			float outWeight;  // weight / rate / time used this frame
			float outRate;
		};

		AnimClipLayer animClipLayers[ANIM_CLIP_LAYERS]{};
		const Game::dvar_t* animClipKeepAds = nullptr;
		const Game::dvar_t* animClipMixGame = nullptr;

		std::uint8_t* GetViewmodelHand(const int hand)
		{
			return reinterpret_cast<std::uint8_t*>(VIEWMODEL_HANDS + hand * VIEWMODEL_HAND_SIZE);
		}

		bool AnyClipActive()
		{
			for (const auto& layer : animClipLayers)
			{
				if (layer.active) return true;
			}
			return false;
		}

		bool IsAdsAnim(const unsigned int anim)
		{
			return anim == Game::WEAP_ANIM_ADS_UP || anim == Game::WEAP_ANIM_ADS_DOWN;
		}

		// Forget the last started anim so WeaponRunXModelAnims restarts
		// whatever the player is doing (the game does the same to re-sync).
		void ResyncViewmodel()
		{
			for (auto hand = 0; hand < Game::NUM_WEAPON_HANDS; ++hand)
			{
				*reinterpret_cast<int*>(GetViewmodelHand(hand) + VIEWMODEL_HAND_PREV_ANIM) = -1;
			}
		}
	}

	void Weapon::AnimClip_Play(const int layerIndex, const unsigned int anim, const float startPct, const float endPct, const float speed, const bool freezeAtEnd, const float weight,
		const int mode, const int speedCurve, const int weightCurve, const int holdMs, const int blendMs)
	{
		if (layerIndex < 0 || layerIndex >= ANIM_CLIP_LAYERS)
		{
			Logger::Print("animclip: layer must be 1-{} (got {})\n", ANIM_CLIP_LAYERS, layerIndex + 1);
			return;
		}

		if (anim < CLIP_FIRST_ANIM || anim > CLIP_LAST_ANIM)
		{
			Logger::Print("animclip: anim must be {}-{} (got {})\n", CLIP_FIRST_ANIM, CLIP_LAST_ANIM, anim);
			return;
		}

		auto start = std::clamp(startPct, 0.0f, 100.0f) / 100.0f;
		auto end = std::clamp(endPct, 0.0f, 100.0f) / 100.0f;
		if (end <= start) end = 1.0f;

		auto& layer = animClipLayers[layerIndex];
		layer.anim = anim;
		layer.start = std::min(start, CLIP_MAX_TIME);
		layer.end = std::min(end, CLIP_MAX_TIME);
		layer.speed = std::clamp(speed, 0.0f, 20.0f);
		layer.weight = std::clamp(weight, 0.0f, 1.0f);
		layer.freezeAtEnd = freezeAtEnd;
		layer.mode = (mode >= 0 && mode < CLIP_MODE_COUNT) ? mode : CLIP_FORWARD;
		layer.speedCurve = (speedCurve >= 0 && speedCurve < CURVE_COUNT) ? speedCurve : CURVE_FLAT;
		layer.weightCurve = (weightCurve >= 0 && weightCurve < CURVE_COUNT) ? weightCurve : CURVE_FLAT;
		layer.holdMs = std::clamp(holdMs, 0, 60000);
		layer.blendMs = std::clamp(blendMs, 0, 10000);

		layer.frozen = false;
		layer.fading = false;
		layer.bounced = false;
		layer.dir = (layer.mode == CLIP_REVERSE) ? -1 : 1;
		layer.pos = (layer.mode == CLIP_REVERSE) ? layer.end : layer.start;
		layer.lastSet = layer.pos;
		layer.outWeight = 0.0f;
		layer.outRate = 0.0f;
		layer.pendingStart = true;
		layer.startedAt = Game::Sys_Milliseconds();
		layer.active = true;
	}

	void Weapon::AnimClip_Stop(const int layerIndex)
	{
		const auto wasActive = AnyClipActive();

		if (layerIndex < 0)
		{
			for (auto& layer : animClipLayers)
			{
				layer.active = false;
				layer.frozen = false;
			}
		}
		else if (layerIndex < ANIM_CLIP_LAYERS)
		{
			animClipLayers[layerIndex].active = false;
			animClipLayers[layerIndex].frozen = false;
		}

		// Last layer gone: hand the viewmodel back to the game
		if (wasActive && !AnyClipActive())
		{
			ResyncViewmodel();
		}
	}

	namespace
	{
		// speed multiplier at progress u (each averages 1.0 over 0..1)
		float ClipSpeedCurve(const int curve, const float u)
		{
			switch (curve)
			{
			case CURVE_IN: return 0.2f + 1.6f * u;
			case CURVE_OUT: return 1.8f - 1.6f * u;
			case CURVE_IN_OUT: return 0.2f + 1.6f * (1.0f - std::fabs(2.0f * u - 1.0f));
			default: return 1.0f;
			}
		}

		// weight multiplier at progress u
		float ClipWeightCurve(const int curve, const float u)
		{
			switch (curve)
			{
			case CURVE_IN: return u;
			case CURVE_OUT: return 1.0f - u;
			case CURVE_IN_OUT: return std::sin(u * 3.14159265f);
			default: return 1.0f;
			}
		}

		// 0..1 through the whole clip (ping-pong counts there and back)
		float ClipProgress(const AnimClipLayer& layer)
		{
			const auto span = std::max(layer.end - layer.start, 0.0001f);
			const auto f = std::clamp((layer.pos - layer.start) / span, 0.0f, 1.0f);
			switch (layer.mode)
			{
			case CLIP_REVERSE: return 1.0f - f;
			case CLIP_PINGPONG:
			case CLIP_PINGPONG_LOOP: return layer.bounced ? 0.5f + 0.5f * (1.0f - f) : 0.5f * f;
			default: return f;
			}
		}
	}

	// The clip runs on its own clock: the engine plays the anim at the
	// layer's rate, we read how far it moved since last frame, then put it
	// where WE want it (forwards, backwards, bounced...). So reverse and
	// ping-pong need no anim lengths - the engine measures the step for us.
	void Weapon::AnimClip_Frame([[maybe_unused]] Game::playerState_s* ps, const int hand)
	{
		if (hand < 0 || hand >= Game::NUM_WEAPON_HANDS || !AnyClipActive()) return;

		auto* obj = *reinterpret_cast<Game::DObj**>(GetViewmodelHand(hand));
		if (!obj || !obj->tree) return;

		auto* tree = DObjGetTree(obj);
		const auto keepAds = animClipKeepAds && animClipKeepAds->current.enabled;
		const auto mixGame = animClipMixGame && animClipMixGame->current.enabled;
		const auto now = Game::Sys_Milliseconds();

		bool used[CLIP_LAST_ANIM + 1]{};
		for (const auto& layer : animClipLayers)
		{
			if (layer.active) used[layer.anim] = true;
		}

		// Runs right after the game's own anim update, so anything it just
		// started this frame (fire, reload...) is silenced before drawing.
		for (auto i = CLIP_FIRST_ANIM; i <= CLIP_LAST_ANIM; ++i)
		{
			if (used[i]) continue;
			if (mixGame) break;
			if (keepAds && IsAdsAnim(i)) continue;
			XAnimSetGoalWeight(obj, i, 0.0f, 0.0f, 1.0f, 0, 0, 0);
		}

		for (auto layerIndex = 0; layerIndex < ANIM_CLIP_LAYERS; ++layerIndex)
		{
			auto& layer = animClipLayers[layerIndex];
			if (!layer.active) continue;
			const auto anim = layer.anim;

			// Left hand (akimbo) just copies what the right hand worked out
			if (hand != 0)
			{
				XAnimSetGoalWeight(obj, anim, layer.outWeight, 0.0f, layer.outRate, 0, 0, 0);
				XAnimSetTime(tree, anim, std::clamp(layer.pos, 0.0f, CLIP_MAX_TIME));
				continue;
			}

			auto restart = 0;
			if (layer.pendingStart)
			{
				layer.pendingStart = false;
				restart = 1;
			}
			else if (!layer.frozen && !layer.fading)
			{
				// how far the engine moved it since we last set it
				auto step = XAnimGetTime(tree, anim) - layer.lastSet;
				if (step < -0.5f) step += 1.0f;   // looping anim wrapped past 1.0
				step = std::clamp(step, 0.0f, 0.1f);

				layer.pos += static_cast<float>(layer.dir) * step;

				auto finished = false;
				if (layer.dir > 0 && layer.pos >= layer.end)
				{
					switch (layer.mode)
					{
					case CLIP_LOOP:
						layer.pos = layer.start;
						break;
					case CLIP_PINGPONG:
					case CLIP_PINGPONG_LOOP:
						layer.pos = layer.end;
						layer.dir = -1;
						layer.bounced = true;
						break;
					default:
						layer.pos = layer.end;
						finished = true;
						break;
					}
				}
				else if (layer.dir < 0 && layer.pos <= layer.start)
				{
					if (layer.mode == CLIP_PINGPONG_LOOP)
					{
						layer.pos = layer.start;
						layer.dir = 1;
						layer.bounced = false;
					}
					else
					{
						layer.pos = layer.start;
						finished = true;
					}
				}

				if (finished)
				{
					if (layer.freezeAtEnd)
					{
						layer.frozen = true;
						layer.frozenAt = now;
					}
					else if (layer.blendMs > 0)
					{
						layer.fading = true;
						layer.fadeAt = now;
					}
					else
					{
						AnimClip_Stop(layerIndex);
						continue;
					}
				}
			}

			// Hold: freeze for holdMs, then fade out (or just let go)
			if (layer.frozen && layer.holdMs > 0 && !layer.fading && now - layer.frozenAt >= layer.holdMs)
			{
				if (layer.blendMs > 0)
				{
					layer.fading = true;
					layer.fadeAt = now;
				}
				else
				{
					AnimClip_Stop(layerIndex);
					continue;
				}
			}

			const auto progress = ClipProgress(layer);
			auto w = layer.weight * ClipWeightCurve(layer.weightCurve, progress);

			if (layer.blendMs > 0)
			{
				const auto sinceStart = static_cast<float>(now - layer.startedAt);
				if (sinceStart < layer.blendMs)
					w *= sinceStart / static_cast<float>(layer.blendMs);

				if (layer.fading)
				{
					const auto out = 1.0f - static_cast<float>(now - layer.fadeAt) / static_cast<float>(layer.blendMs);
					if (out <= 0.0f)
					{
						AnimClip_Stop(layerIndex);
						continue;
					}
					w *= out;
				}
			}

			// tiny floor so a faded-to-zero layer still keeps its slot in the tree
			w = std::clamp(w, 0.001f, 1.0f);
			const auto rate = (layer.frozen || layer.fading) ? 0.0f : layer.speed * ClipSpeedCurve(layer.speedCurve, progress);

			layer.outWeight = w;
			layer.outRate = rate;

			XAnimSetGoalWeight(obj, anim, w, 0.0f, rate, 0, 0, restart);
			const auto t = std::clamp(layer.pos, 0.0f, CLIP_MAX_TIME);
			XAnimSetTime(tree, anim, t);
			layer.lastSet = t;
		}
	}

	__declspec(naked) void Weapon::WeaponRunXModelAnims_Stub()
	{
		__asm
		{
			// entry: eax = hand, [esp] = return, [esp + 4] = playerState
			push eax                    // hand
			push dword ptr [esp + 8]    // playerState
			call WeaponRunXModelAnims_Hook   // (playerState, hand)
			add esp, 8
			ret
		}
	}

	void Weapon::AddAnimClip()
	{
		animClipKeepAds = Game::Dvar_RegisterBool("animclip_keepads", true, Game::DVAR_ARCHIVE,
			"Anim clips leave the ADS anims to the game, so you can aim down sights during a clip");
		animClipMixGame = Game::Dvar_RegisterBool("animclip_mixgame", false, Game::DVAR_ARCHIVE,
			"Anim clips blend with the game's own viewmodel anims (fire, reload...) instead of hiding them");

		// AnimClipPlay(<anim>, [startPct = 0], [endPct = 100], [speed = 1.0], [freeze = 0], [layer = 1], [weight = 1.0])
		GSC::Script::AddFunction("AnimClipPlay", []
			{
				const auto numParams = Game::Scr_GetNumParam();
				const auto anim = static_cast<unsigned int>(Game::Scr_GetInt(0));
				const auto start = numParams > 1 ? Game::Scr_GetFloat(1) : 0.0f;
				const auto end = numParams > 2 ? Game::Scr_GetFloat(2) : 100.0f;
				const auto speed = numParams > 3 ? Game::Scr_GetFloat(3) : 1.0f;
				const auto freeze = numParams > 4 ? Game::Scr_GetInt(4) != 0 : false;
				const auto layer = numParams > 5 ? Game::Scr_GetInt(5) - 1 : 0;
				const auto weight = numParams > 6 ? Game::Scr_GetFloat(6) : 1.0f;
				const auto mode = numParams > 7 ? Game::Scr_GetInt(7) : 0;
				const auto speedCurve = numParams > 8 ? Game::Scr_GetInt(8) : 0;
				const auto weightCurve = numParams > 9 ? Game::Scr_GetInt(9) : 0;
				const auto holdMs = numParams > 10 ? Game::Scr_GetInt(10) : 0;
				const auto blendMs = numParams > 11 ? Game::Scr_GetInt(11) : 0;
				AnimClip_Play(layer, anim, start, end, speed, freeze, weight, mode, speedCurve, weightCurve, holdMs, blendMs);
			});

		// AnimClipStop([layer]);   no layer = stop all
		GSC::Script::AddFunction("AnimClipStop", []
			{
				AnimClip_Stop(Game::Scr_GetNumParam() > 0 ? Game::Scr_GetInt(0) - 1 : -1);
			});

		// playing = AnimClipIsPlaying([layer]);   no layer = any
		GSC::Script::AddFunction("AnimClipIsPlaying", []
			{
				if (Game::Scr_GetNumParam() > 0)
				{
					const auto layer = Game::Scr_GetInt(0) - 1;
					Game::Scr_AddInt(layer >= 0 && layer < ANIM_CLIP_LAYERS && animClipLayers[layer].active ? 1 : 0);
				}
				else
				{
					Game::Scr_AddInt(AnyClipActive() ? 1 : 0);
				}
			});

		// Console: animclip <anim> [startPct] [endPct] [speed] [freeze] [layer] [weight] [mode] [speedcurve] [weightcurve] [holdMs] [blendMs]
		//   mode: 0 forward, 1 reverse, 2 ping-pong, 3 loop, 4 ping-pong loop
		//   curves: 0 flat, 1 in, 2 out, 3 in-out
		Command::Add("animclip", [](const Command::Params* params)
			{
				if (params->size() < 2)
				{
					Logger::Print("usage: animclip <anim 1-36> [startPct 0-100] [endPct 0-100] [speed] [freeze 0/1] [layer 1-{}] [weight 0-1] [mode 0-4] [speedcurve 0-3] [weightcurve 0-3] [holdMs] [blendMs]\n", ANIM_CLIP_LAYERS);
					return;
				}

				const auto anim = static_cast<unsigned int>(std::strtoul(params->get(1), nullptr, 10));
				const auto start = params->size() > 2 ? std::strtof(params->get(2), nullptr) : 0.0f;
				const auto end = params->size() > 3 ? std::strtof(params->get(3), nullptr) : 100.0f;
				const auto speed = params->size() > 4 ? std::strtof(params->get(4), nullptr) : 1.0f;
				const auto freeze = params->size() > 5 ? std::strtoul(params->get(5), nullptr, 10) != 0 : false;
				const auto layer = params->size() > 6 ? std::atoi(params->get(6)) - 1 : 0;
				const auto weight = params->size() > 7 ? std::strtof(params->get(7), nullptr) : 1.0f;
				const auto mode = params->size() > 8 ? std::atoi(params->get(8)) : 0;
				const auto speedCurve = params->size() > 9 ? std::atoi(params->get(9)) : 0;
				const auto weightCurve = params->size() > 10 ? std::atoi(params->get(10)) : 0;
				const auto holdMs = params->size() > 11 ? std::atoi(params->get(11)) : 0;
				const auto blendMs = params->size() > 12 ? std::atoi(params->get(12)) : 0;
				AnimClip_Play(layer, anim, start, end, speed, freeze, weight, mode, speedCurve, weightCurve, holdMs, blendMs);
			});

		// animclip_stop [layer]
		Command::Add("animclip_stop", [](const Command::Params* params)
			{
				AnimClip_Stop(params->size() > 1 ? std::atoi(params->get(1)) - 1 : -1);
			});

		// WeaponRunXModelAnims call (per hand, per frame)
		Utils::Hook(0x47F55C, WeaponRunXModelAnims_Stub, HOOK_CALL).install()->quick();
	}

	// =====================================================================
	// Per-player weapon overrides
	// ---------------------------------------------------------------------
	// WeapDefSet edits the weapon for EVERY player. These edit it for ONE
	// player: the values are written into the gun right before that
	// player's weapon update runs and restored right after, so each player
	// sees their own reload / rechamber / raise / drop / sprint / fire times.
	//
	// Applied around:
	//   PM_Weapon            (server ClientThink + the host's own prediction,
	//                         both go through PmoveSingle -> our stub)
	//   WeaponRunXModelAnims (host viewmodel anim rates, e.g. a slower reload
	//                         anim for a slower reload time)
	// Numeric fields only. Anim fields stay global (they are host-only
	// visuals anyway). Other players' own clients keep stock values unless
	// they run this DLL, same as before.
	// =====================================================================
	namespace
	{
		constexpr int OVERRIDE_CLIENTS = 64;

		struct PlayerOverride
		{
			unsigned int weapIndex;
			const Game::cspField_t* field;
			float value;
		};

		struct AppliedValue
		{
			std::uint8_t* address;
			std::uint32_t raw;
			int size;
		};

		std::vector<PlayerOverride> playerOverrides[OVERRIDE_CLIENTS];

		bool ValidClient(const int clientNum)
		{
			return clientNum >= 0 && clientNum < OVERRIDE_CLIENTS;
		}

		void ApplyPlayerOverrides(const int clientNum, std::vector<AppliedValue>& saved)
		{
			if (!ValidClient(clientNum) || playerOverrides[clientNum].empty()) return;

			for (const auto& o : playerOverrides[clientNum])
			{
				auto* address = GetFieldAddress(o.weapIndex, o.field);
				if (!address) continue;

				const auto size = FieldSize(o.field);
				saved.push_back(AppliedValue{ address, ReadRaw(address, size), size });
				WriteNumeric(address, o.field->iFieldType, o.value);
			}
		}

		void RestorePlayerOverrides(std::vector<AppliedValue>& saved)
		{
			// reverse order: two overrides on one shared address restore cleanly
			for (auto it = saved.rbegin(); it != saved.rend(); ++it)
			{
				std::memcpy(it->address, &it->raw, it->size);
			}
			saved.clear();
		}

		PlayerOverride* FindOverride(const int clientNum, const unsigned int weapIndex, const Game::cspField_t* field)
		{
			for (auto& o : playerOverrides[clientNum])
			{
				if (o.weapIndex == weapIndex && o.field == field) return &o;
			}
			return nullptr;
		}

		int GetPlayerArg(const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
			return ent ? ent->s.number : -1;
		}

		// Per-player "no rechamber" guns (weapon indices), see NoRechamber_*
		std::vector<unsigned int> noRechamberWeapons[OVERRIDE_CLIENTS];

		// Per-player "server-only Sleight of Hand", see ServerSoh_*
		bool serverSoh[OVERRIDE_CLIENTS]{};

		// Per-player fake-lunge guns (weapon indices), see FakeLunge_*
		std::vector<unsigned int> fakeLungeWeapons[OVERRIDE_CLIENTS];
		bool fakeLungeActive[OVERRIDE_CLIENTS]{};   // a lunge we started is running

		// ---- This game's OWN copy (players running this dll) -------------
		// The server sends these with console commands (omen_norechamber,
		// omen_fakelunge, omen_slow / omen_sohclass) so the player's own game predicts its
		// weapon exactly like the server runs it - no corrections, no glitch.
		// Only used while this game is a client of someone else's server and
		// only for the local player's own prediction.
		std::vector<unsigned int> localNoRechamber;
		std::vector<unsigned int> localFakeLunge;
		std::vector<unsigned int> localSlow;    // Slow Reload guns
		bool localSohClass = false;             // this life's class has Sleight of Hand

		void ClearLocalMods()
		{
			localNoRechamber.clear();
			localFakeLunge.clear();
			localSlow.clear();
			localSohClass = false;
		}

		bool LocalPredict(const int clientNum)
		{
			if (Game::SV_Loaded()) return false;            // we're the server: GSC drives the tables
			if (!Game::CL_IsCgameInitialized()) return false;
			return Game::cgArray[0].clientNum == clientNum;
		}

		bool InList(const std::vector<unsigned int>& list, const unsigned int weapIndex)
		{
			return weapIndex && std::find(list.begin(), list.end(), weapIndex) != list.end();
		}

		bool AnyFakeLunge(const int clientNum)
		{
			if (!ValidClient(clientNum)) return false;
			if (!fakeLungeWeapons[clientNum].empty()) return true;
			return !localFakeLunge.empty() && LocalPredict(clientNum);
		}

		bool AnyNoRechamber(const int clientNum)
		{
			if (!ValidClient(clientNum)) return false;
			if (!noRechamberWeapons[clientNum].empty()) return true;
			return !localNoRechamber.empty() && LocalPredict(clientNum);
		}

		bool HasFakeLunge(const int clientNum, const unsigned int weapIndex)
		{
			if (!ValidClient(clientNum) || !weapIndex) return false;
			if (InList(fakeLungeWeapons[clientNum], weapIndex)) return true;
			return InList(localFakeLunge, weapIndex) && LocalPredict(clientNum);
		}

		bool HasNoRechamber(const int clientNum, const unsigned int weapIndex)
		{
			if (!ValidClient(clientNum) || !weapIndex) return false;
			if (InList(noRechamberWeapons[clientNum], weapIndex)) return true;
			return InList(localNoRechamber, weapIndex) && LocalPredict(clientNum);
		}

		void SetInList(std::vector<unsigned int>& list, const unsigned int weapIndex, const bool on)
		{
			const auto it = std::find(list.begin(), list.end(), weapIndex);
			if (on && it == list.end()) list.push_back(weapIndex);
			else if (!on && it != list.end()) list.erase(it);
		}
	}

	bool Weapon::WeapDef_SetFor(const int clientNum, const unsigned int weapIndex, const char* fieldName, const char* value)
	{
		const auto* field = FindWeaponField(fieldName);
		if (!weapIndex || !field || !value) return false;

		// anims are host-only visuals: keep using the global swap
		if (IsAnimField(field)) return WeapDef_Set(weapIndex, fieldName, value);

		if (!ValidClient(clientNum) || !IsNumericField(field)) return false;

		const auto number = std::strtof(value, nullptr);
		if (auto* existing = FindOverride(clientNum, weapIndex, field))
		{
			existing->value = number;
		}
		else
		{
			playerOverrides[clientNum].push_back(PlayerOverride{ weapIndex, field, number });
		}
		return true;
	}

	void Weapon::WeapDef_ResetFor(const int clientNum, const unsigned int weapIndex, const char* fieldName)
	{
		if (!ValidClient(clientNum)) return;

		const auto* field = fieldName ? FindWeaponField(fieldName) : nullptr;
		if (field && IsAnimField(field))
		{
			WeapDef_Reset(weapIndex, fieldName);
			return;
		}

		auto& list = playerOverrides[clientNum];
		std::erase_if(list, [&](const PlayerOverride& o)
			{
				if (o.weapIndex != weapIndex) return false;
				return !field || o.field == field;
			});

		if (!fieldName)
		{
			// whole weapon: anim swaps too
			for (auto anim = 0u; anim < static_cast<unsigned int>(Game::NUM_WEAP_ANIMS); ++anim)
			{
				SwapWeaponAnim(weapIndex, anim, nullptr);
			}
		}
	}

	void Weapon::WeapDef_ClearFor(const int clientNum)
	{
		if (ValidClient(clientNum)) playerOverrides[clientNum].clear();
	}

	void Weapon::WeaponRunXModelAnims_Hook(Game::playerState_s* ps, const int hand)
	{
		std::vector<AppliedValue> saved;
		ApplyPlayerOverrides(ps ? ps->clientNum : -1, saved);
		const auto sohApplied = ServerSoh_Begin(ps, false);   // viewmodel keeps the Marathon-speed anim
		CallWeaponRunXModelAnims(ps, hand);
		ServerSoh_End(ps, sohApplied);
		RestorePlayerOverrides(saved);

		AnimClip_Frame(ps, hand);
	}

	__declspec(naked) void Weapon::CallWeaponRunXModelAnims([[maybe_unused]] Game::playerState_s* ps, [[maybe_unused]] int hand)
	{
		__asm
		{
			// cdecl (ps, hand) -> original: eax = hand, push ps
			mov eax, [esp + 8]          // hand
			push dword ptr [esp + 4]    // ps
			mov ecx, 0x59B5E0           // WeaponRunXModelAnims
			call ecx
			add esp, 4
			ret
		}
	}

	void Weapon::AddPerPlayerWeapDef()
	{
		AssertOffset(Game::playerState_s, clientNum, 0x104);

		Events::OnClientDisconnect([](const int clientNum)
			{
				WeapDef_ClearFor(clientNum);
				if (ValidClient(clientNum))
				{
					noRechamberWeapons[clientNum].clear();
					fakeLungeWeapons[clientNum].clear();
					fakeLungeActive[clientNum] = false;
					serverSoh[clientNum] = false;
				}
			});

		// map change / fast_restart: weapon indices can change, scripts re-apply on spawn
		Events::OnVMShutdown([]
			{
				for (auto& list : playerOverrides) list.clear();
				for (auto& list : noRechamberWeapons) list.clear();
				for (auto& list : fakeLungeWeapons) list.clear();
				for (auto& active : fakeLungeActive) active = false;
				for (auto& soh : serverSoh) soh = false;
			});

		// self WeapDefSetFor(<weapon>, <field>, <value>)   - only this player
		GSC::Script::AddMethod("WeapDefSetFor", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				const auto weapIndex = GetWeaponArg(0);
				const auto* field = GetFieldArg(1);
				if (!weapIndex || !field) return;

				if (IsAnimField(field))
				{
					WeapDef_SetFor(clientNum, weapIndex, field->szName, Game::Scr_GetString(2));
				}
				else
				{
					WeapDef_SetFor(clientNum, weapIndex, field->szName, std::to_string(Game::Scr_GetFloat(2)).c_str());
				}
			});

		// value = self WeapDefGetFor(<weapon>, <field>)   - this player's value
		GSC::Script::AddMethod("WeapDefGetFor", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				const auto weapIndex = GetWeaponArg(0);
				const auto* field = GetFieldArg(1);
				if (!weapIndex || !field)
				{
					Game::Scr_AddInt(0);
					return;
				}

				if (IsAnimField(field))
				{
					Game::Scr_AddString(GetCurrentAnimName(weapIndex, AnimIndexOf(field)));
					return;
				}

				if (ValidClient(clientNum))
				{
					if (const auto* o = FindOverride(clientNum, weapIndex, field))
					{
						Game::Scr_AddFloat(o->value);
						return;
					}
				}

				auto* address = GetFieldAddress(weapIndex, field);
				Game::Scr_AddFloat(address ? ReadNumeric(address, field->iFieldType) : 0.0f);
			});

		// self WeapDefResetFor(<weapon>, [field])   - this player back to stock
		GSC::Script::AddMethod("WeapDefResetFor", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				const auto weapIndex = GetWeaponArg(0);
				if (!weapIndex) return;
				WeapDef_ResetFor(clientNum, weapIndex, Game::Scr_GetNumParam() > 1 ? Game::Scr_GetString(1) : nullptr);
			});

		// self WeapDefClearFor()   - every per-player override of this player
		GSC::Script::AddMethod("WeapDefClearFor", [](const Game::scr_entref_t entref)
			{
				WeapDef_ClearFor(GetPlayerArg(entref));
			});
	}

	void Weapon::PM_Weapon_stub(Game::pmove_s* pm, Game::pml_t* pml)
	{
		// per-player weapon overrides for this player's update only
		std::vector<AppliedValue> perPlayer;
		ApplyPlayerOverrides((pm && pm->ps) ? pm->ps->clientNum : -1, perPlayer);

		if (BGDisableDoubleTaps && BGDisableDoubleTaps->current.enabled)
		{
			if (pm && pm->ps)
			{
				if (pm->cmd.weapon == pm->ps->weapCommon.weapon &&
					(pm->ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState == Game::WEAPON_DROPPING
						|| pm->ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState == Game::WEAPON_DROPPING_QUICK
						|| pm->ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState == Game::WEAPON_DROPPING_ALT))
				{
					// change each hand's weapon data
					for (int index = 0; index < Game::NUM_WEAPON_HANDS; index++)
					{
						pm->ps->weapState[index].weaponState = Game::WEAPON_RAISING;
						pm->ps->weapState[index].weaponTime = Game::BG_GetWeaponDef(pm->ps->weapCommon.weapon)->quickRaiseTime;
						pm->ps->weapState[index].weapAnim = Game::WEAP_ANIM_QUICK_DROP;
					}
				}
			}
		}

		NoRechamber_BeforeWeapon(pm);
		FakeLunge_BeforeWeapon(pm);
		const auto sohApplied = ServerSoh_Begin(pm ? pm->ps : nullptr, true);

		Game::PM_Weapon(pm, pml);

		ServerSoh_End(pm ? pm->ps : nullptr, sohApplied);
		RestorePlayerOverrides(perPlayer);

		NoRechamber_AfterWeapon(pm ? pm->ps : nullptr);
		FakeLunge_AfterWeapon(pm ? pm->ps : nullptr);
	}

	// =====================================================================
	// Per-player fake lunge (OMEN test option)
	// ---------------------------------------------------------------------
	// A real lunge is decided by the player's own game: when an enemy is in
	// range it puts a lunge distance + direction into the input it sends
	// (usercmd meleeChargeDist / meleeChargeYaw) and the weapon code turns
	// the melee into a melee-charge. Here, on the SERVER, a melee press with
	// a listed gun gets a 1-unit lunge toward where the player is looking
	// filled in, so the game runs its own melee-charge: lunge anim, then the
	// gun comes back by itself. Only this player, nothing installed needed.
	//
	// No pull: the made-up target is 1 unit away, so the lunge movement is
	// 1 unit - not visible. (Zeroing the distance after the weapon update
	// turned it back into a plain melee.) It's still a real melee - it can
	// hit what's right in front of you. Each lunge is logged to the console
	// (fakelunge: ...) while this is being tested.
	//
	// GSC:  self SetFakeLunge(<weapon>, <on 0/1>)   self ClearFakeLunge()
	// =====================================================================
	void Weapon::FakeLunge_BeforeWeapon(Game::pmove_s* pm)
	{
		if (!pm || !pm->ps) return;
		auto* ps = pm->ps;
		const auto clientNum = ps->clientNum;
		if (!AnyFakeLunge(clientNum)) return;
		if (!HasFakeLunge(clientNum, ps->weapCommon.weapon)) return;

		// only on the press, not while it's held
		const auto pressed = (pm->cmd.buttons & Game::CMD_BUTTON_MELEE) != 0;
		const auto wasPressed = (pm->oldcmd.buttons & Game::CMD_BUTTON_MELEE) != 0;
		if (!pressed || wasPressed) return;

		// the player's own game already found a real target: leave it alone
		if (pm->cmd.meleeChargeDist != 0) return;

		pm->cmd.meleeChargeDist = 1;
		pm->cmd.meleeChargeYaw = ps->viewangles[1];

		// own-game prediction replays inputs every frame - log on the server only
		if (LocalPredict(clientNum)) return;
		fakeLungeActive[clientNum] = true;

		Logger::Print("fakelunge: client {} melee press - lunge requested (weapon state {}, anim {})\n",
			clientNum, ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState, ps->weapState[Game::WEAPON_HAND_RIGHT].weapAnim);
	}

	void Weapon::FakeLunge_AfterWeapon(Game::playerState_s* ps)
	{
		if (!ps) return;
		const auto clientNum = ps->clientNum;
		if (!ValidClient(clientNum) || !fakeLungeActive[clientNum]) return;

		const auto state = ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState;

		// what the game made of it - logged once per lunge (first frame after the press)
		if (fakeLungeActive[clientNum])
		{
			Logger::Print("fakelunge: client {} after weapon update - state {}, anim {} (melee charge anim = {}), chargeDist {}, chargeTime {}\n",
				clientNum, state, ps->weapState[Game::WEAPON_HAND_RIGHT].weapAnim, static_cast<int>(Game::WEAP_ANIM_MELEE_CHARGE),
				ps->meleeChargeDist, ps->meleeChargeTime);
		}

		// The 1-unit lunge is left alone (zeroing it here turned it back into
		// a plain melee) - a 1-unit pull isn't visible.
		fakeLungeActive[clientNum] = false;
	}

	// =====================================================================
	// Per-player "server-only Sleight of Hand" (OMEN Slow Reload)
	// ---------------------------------------------------------------------
	// OMEN takes Sleight of Hand away from the player (the perk bit their game
	// receives is off, so it plays the full Marathon reload animation). With
	// this on, the server puts the SoH bit back ONLY while it runs that
	// player's weapon update, and clears it again right after - so reload,
	// ammo-in, swapping during a reload and anything else the engine ties to
	// SoH behave exactly like a real SoH class (SoH NAC cfgs line up), while
	// the state sent to the player's game still says no SoH.
	//
	// GSC:  self SetServerSoh(<on 0/1>)
	// =====================================================================
	bool Weapon::ServerSoh_Begin(Game::playerState_s* ps, const bool allowLocal)
	{
		if (!ps) return false;
		const auto clientNum = ps->clientNum;
		if (!ValidClient(clientNum)) return false;
		// own game: same rule the server's OMEN script uses - SoH class,
		// holding a Slow Reload gun
		const auto wanted = serverSoh[clientNum]
			|| (allowLocal && localSohClass && InList(localSlow, ps->weapCommon.weapon) && LocalPredict(clientNum));
		if (!wanted) return false;

		constexpr auto bit = 1u << Game::PERK_FASTRELOAD;
		if (ps->perks[0] & bit) return false;   // already has it for real

		ps->perks[0] |= bit;
		return true;
	}

	void Weapon::ServerSoh_End(Game::playerState_s* ps, const bool applied)
	{
		if (!ps || !applied) return;
		ps->perks[0] &= ~(1u << Game::PERK_FASTRELOAD);
	}

	void Weapon::AddLocalModCommands()
	{
		// omen_hello: the OMEN server asks "do you have the OMEN dll?".
		// We answer with a client command it can catch. A stock game doesn't
		// know omen_hello and passes it to the server instead - also an answer.
		Command::Add("omen_hello", []([[maybe_unused]] const Command::Params* params)
			{
				Game::CL_AddReliableCommand(0, "omen_dll_yes 2");
			});

		// The player's own game: same per-player mods the server runs for
		// them, so its prediction matches (OMEN sends these to dll players).
		//   omen_norechamber <weapon> <0/1>     omen_fakelunge <weapon> <0/1>
		//   omen_slow <weapon> <0/1>            omen_sohclass <0/1>
		//   omen_localmods_clear
		Command::Add("omen_norechamber", [](const Command::Params* params)
			{
				if (params->size() < 2) return;
				const auto weapIndex = Game::G_GetWeaponIndexForName(params->get(1));
				if (!weapIndex) return;
				SetInList(localNoRechamber, weapIndex, params->size() < 3 || std::atoi(params->get(2)) != 0);
			});

		Command::Add("omen_fakelunge", [](const Command::Params* params)
			{
				if (params->size() < 2) return;
				const auto weapIndex = Game::G_GetWeaponIndexForName(params->get(1));
				if (!weapIndex) return;
				SetInList(localFakeLunge, weapIndex, params->size() < 3 || std::atoi(params->get(2)) != 0);
			});

		Command::Add("omen_slow", [](const Command::Params* params)
			{
				if (params->size() < 2) return;
				const auto weapIndex = Game::G_GetWeaponIndexForName(params->get(1));
				if (!weapIndex) return;
				SetInList(localSlow, weapIndex, params->size() < 3 || std::atoi(params->get(2)) != 0);
			});

		Command::Add("omen_sohclass", [](const Command::Params* params)
			{
				localSohClass = params->size() < 2 || std::atoi(params->get(1)) != 0;
			});

		Command::Add("omen_localmods_clear", []
			{
				ClearLocalMods();
			});

		// leaving a server / loading a map: weapon indices can change,
		// the server sends everything again on spawn
		Events::OnCLDisconnected([](bool) { ClearLocalMods(); });
		Events::OnCGameInit([] { ClearLocalMods(); });
	}

	void Weapon::AddServerSoh()
	{
		// self SetServerSoh(<on 0/1>)   - only this player
		GSC::Script::AddMethod("SetServerSoh", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				if (!ValidClient(clientNum)) return;
				serverSoh[clientNum] = Game::Scr_GetNumParam() < 1 || Game::Scr_GetInt(0) != 0;
			});
	}

	void Weapon::AddFakeLunge()
	{
		// self SetFakeLunge(<weapon>, <on 0/1>)   - only this player, only that gun
		GSC::Script::AddMethod("SetFakeLunge", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				if (!ValidClient(clientNum)) return;

				const auto weapIndex = GetWeaponArg(0);
				if (!weapIndex) return;

				const auto on = Game::Scr_GetNumParam() < 2 || Game::Scr_GetInt(1) != 0;
				auto& list = fakeLungeWeapons[clientNum];
				const auto it = std::find(list.begin(), list.end(), weapIndex);

				if (on && it == list.end()) list.push_back(weapIndex);
				else if (!on && it != list.end()) list.erase(it);
			});

		// self ClearFakeLunge()   - every gun of this player back to normal melee
		GSC::Script::AddMethod("ClearFakeLunge", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				if (!ValidClient(clientNum)) return;
				fakeLungeWeapons[clientNum].clear();
				fakeLungeActive[clientNum] = false;
			});
	}

	// =====================================================================
	// Per-player "no rechamber" (OMEN No Cock)
	// ---------------------------------------------------------------------
	// Doesn't touch the weapon's timings. It changes only THIS player's own
	// weapon state, which the server already sends to their game, so a stock
	// IW4x game follows it without needing anything installed.
	//
	//  - One shot still plays the normal bolt (nothing changes on its own).
	//  - Fire, sprint or switch weapons while the bolt is still cycling:
	//    the gun is marked ready right there (state READY, timer 0), so the
	//    shot goes out / you come out of the sprint ready / the swap starts.
	//  - A gun that is NOT in your hands never owes a cock-back: its
	//    "needs rechamber" flag is cleared, so pulling it back out is clean.
	//
	// The held gun's flag is left alone while its bolt cycles normally (the
	// game may use it to play that bolt) and cleared together with the
	// READY above.
	//
	// GSC:  self SetNoRechamber(<weapon>, <on 0/1>)   self ClearNoRechamber()
	// =====================================================================
	void Weapon::NoRechamber_BeforeWeapon(Game::pmove_s* pm)
	{
		if (!pm || !pm->ps) return;
		auto* ps = pm->ps;
		const auto clientNum = ps->clientNum;
		if (!AnyNoRechamber(clientNum)) return;

		const auto weapon = ps->weapCommon.weapon;
		if (!HasNoRechamber(clientNum, weapon)) return;

		const auto wantsFire = (pm->cmd.buttons & Game::CMD_BUTTON_ATTACK) != 0;
		const auto wantsSprint = (pm->cmd.buttons & Game::CMD_BUTTON_SPRINT) != 0;
		const auto cmdWeapon = static_cast<unsigned int>(pm->cmd.weapon);
		const auto wantsSwitch = cmdWeapon != 0 && cmdWeapon != weapon;
		if (!wantsFire && !wantsSprint && !wantsSwitch) return;

		auto cleared = false;
		for (auto& ws : ps->weapState)
		{
			if (ws.weaponState == Game::WEAPON_RECHAMBERING)
			{
				ws.weaponState = Game::WEAPON_READY;
				ws.weaponTime = 0;
				cleared = true;
			}
		}

		if (!cleared) return;

		for (auto i = 0; i < static_cast<int>(std::size(ps->weaponsEquipped)); ++i)
		{
			if (ps->weaponsEquipped[i] == weapon)
			{
				ps->weapEquippedData[i].needsRechamber[0] = false;
				ps->weapEquippedData[i].needsRechamber[1] = false;
			}
		}
	}

	void Weapon::NoRechamber_AfterWeapon(Game::playerState_s* ps)
	{
		if (!ps) return;
		const auto clientNum = ps->clientNum;
		if (!AnyNoRechamber(clientNum)) return;

		const auto held = ps->weapCommon.weapon;
		for (auto i = 0; i < static_cast<int>(std::size(ps->weaponsEquipped)); ++i)
		{
			const auto weapIndex = ps->weaponsEquipped[i];
			if (!weapIndex || weapIndex == held) continue;
			if (!HasNoRechamber(clientNum, weapIndex)) continue;

			ps->weapEquippedData[i].needsRechamber[0] = false;
			ps->weapEquippedData[i].needsRechamber[1] = false;
		}
	}

	void Weapon::AddNoRechamber()
	{
		// self SetNoRechamber(<weapon>, <on 0/1>)   - only this player, only that gun
		GSC::Script::AddMethod("SetNoRechamber", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				if (!ValidClient(clientNum)) return;

				const auto weapIndex = GetWeaponArg(0);
				if (!weapIndex) return;

				const auto on = Game::Scr_GetNumParam() < 2 || Game::Scr_GetInt(1) != 0;
				auto& list = noRechamberWeapons[clientNum];
				const auto it = std::find(list.begin(), list.end(), weapIndex);

				if (on && it == list.end()) list.push_back(weapIndex);
				else if (!on && it != list.end()) list.erase(it);
			});

		// self ClearNoRechamber()   - every gun of this player back to normal
		GSC::Script::AddMethod("ClearNoRechamber", [](const Game::scr_entref_t entref)
			{
				const auto clientNum = GetPlayerArg(entref);
				if (ValidClient(clientNum)) noRechamberWeapons[clientNum].clear();
			});
	}

	Weapon::Weapon()
	{
		if (!Flags::HasFlag("steamdemo") && !Flags::HasFlag("retaildemo"))
		{
			// Steam version uses a limit of 1400 weapons and other clients use the default limit of 1200 weapons
			PatchLimit();
		}

		// BG_LoadWEaponCompleteDef_FastFile
		Utils::Hook(0x57B650, LoadWeaponCompleteDef, HOOK_JUMP).install()->quick();
		// Disable warning if raw weapon file cannot be found
		Utils::Hook::Nop(0x57AF60, 5);

		// weapon asset existence check
		Utils::Hook::Nop(0x408228, 5); // find asset header
		Utils::Hook::Nop(0x408230, 5); // is asset default
		Utils::Hook::Nop(0x40823A, 2); // jump

		// Automatically register weapons, even if the level is not loading
		Utils::Hook::Nop(0x49E547, 2);
		Utils::Hook::Set<BYTE>(0x44F240, 0xEB);

		// Skip double loading for fs_game
		Utils::Hook::Set<BYTE>(0x4081FD, 0xEB);

		// Weapon swap fix
		Utils::Hook::Nop(0x4B3670, 5);
		Utils::Hook(0x57B4F0, LoadNoneWeaponHookStub, HOOK_JUMP).install()->quick();

		// MW3-style weapon swapping mechanics
		BGDisableDoubleTaps = Game::Dvar_RegisterBool("bg_disableDoubleTaps", false, Game::DVAR_CODINFO, "Enables MW3-style weapon swapping mechanics");
		Utils::Hook(0x574960, PM_Weapon_stub, HOOK_CALL).install()->quick(); // PmoveSingle
		Utils::Hook(0x574B69, PM_Weapon_stub, HOOK_CALL).install()->quick(); // ^
		Utils::Hook(0x574AB2, PM_Weapon_stub, HOOK_CALL).install()->quick(); // ^

		// Clear weapons independently from fs_game
		Utils::Hook::Nop(0x452C1D, 2);
		Utils::Hook::Nop(0x452C24, 5);

		Utils::Hook(0x59E341, CG_UpdatePrimaryForAltModeWeapon_Stub, HOOK_JUMP).install()->quick();
		Utils::Hook(0x48BB25, CG_SelectWeaponIndex_Stub, HOOK_JUMP).install()->quick();

		AssertOffset(Game::playerState_s, Game::playerState_s::weapCommon.weapFlags, 0x2BC);
		Utils::Hook(0x56E825, WeaponEntCanBeGrabbed_Stub, HOOK_JUMP).install()->quick();

		// Javelin fix (PM_Weapon_OffHandEnd)
		AssertOffset(Game::playerState_s, grenadeTimeLeft, 0x34);
		BGWeaponOffHandFix = Game::Dvar_RegisterBool("bg_weaponOffHandFix", true, Game::DVAR_CODINFO, "Reset grenadeTimeLeft after using off hand weapon");
		Utils::Hook(0x578F52, JavelinResetHook_Stub, HOOK_JUMP).install()->quick();

		CGRecoilMultiplier = Game::Dvar_RegisterFloat("cg_recoilMultiplier",
			1.0f, 0.0f, 1000.0f, Game::DVAR_CHEAT,
			"The scale applied to the player recoil when firing");
		Utils::Hook(0x44D90B, BG_WeaponFireRecoil_Stub, HOOK_CALL).install()->quick();
		Utils::Hook(0x4FB2D7, BG_WeaponFireRecoil_Stub, HOOK_CALL).install()->quick();

		AddScriptMethods();
		AddAnimClip();
		AddWeapDefEditing();
		AddPerPlayerWeapDef();
		AddNoRechamber();
		AddFakeLunge();
		AddServerSoh();
		AddLocalModCommands();
	}
}

#pragma once



namespace Components
{
	class Weapon : public Component
	{
	public:
		Weapon();
		static const int BASEGAME_WEAPON_LIMIT = 1200;

		// Increase the weapon limit
		static const int WEAPON_LIMIT = 2400;

		static const int ADDED_WEAPONS = WEAPON_LIMIT - BASEGAME_WEAPON_LIMIT;
		static const Game::dvar_t* BGDisableDoubleTaps;

	private:
		static const Game::dvar_t* BGWeaponOffHandFix;
		static const Game::dvar_t* CGRecoilMultiplier;

		static Game::WeaponCompleteDef* LoadWeaponCompleteDef(const char* name);
		static void PatchLimit();
		static void* LoadNoneWeaponHook();
		static void LoadNoneWeaponHookStub();

		static void SaveRegisteredWeapons();

		static void PatchHintStrings();

		static void CG_UpdatePrimaryForAltModeWeapon_Stub();
		static void CG_SelectWeaponIndex_Stub();

		static void JavelinResetHook_Stub();

		static void WeaponEntCanBeGrabbed_Stub();

		static void BG_WeaponFireRecoil_Stub(void* ps, float* recoilSpeed, float* kickAVel, unsigned int* holdrand, Game::PlayerHandIndex hand);

		static void PlayerCmd_InitialWeaponRaise(Game::scr_entref_t entref);
		static void PlayerCmd_FreezeControlsAllowLook(Game::scr_entref_t entref);

		static void AddScriptMethods();
		// Live weapon editing by weapon-file field name
		static bool WeapDef_Set(unsigned int weapIndex, const char* fieldName, const char* value);
		static void WeapDef_Reset(unsigned int weapIndex, const char* fieldName);
		static void WeapDef_ResetAll();
		static void AddWeapDefEditing();

		// Per-player weapon overrides
		static bool WeapDef_SetFor(int clientNum, unsigned int weapIndex, const char* fieldName, const char* value);
		static void WeapDef_ResetFor(int clientNum, unsigned int weapIndex, const char* fieldName);
		static void WeapDef_ClearFor(int clientNum);
		static void WeaponRunXModelAnims_Hook(Game::playerState_s* ps, int hand);
		static void CallWeaponRunXModelAnims(Game::playerState_s* ps, int hand);
		static void AddPerPlayerWeapDef();

		// Viewmodel anim clip player
		static void AnimClip_Play(int layerIndex, unsigned int anim, float startPct, float endPct, float speed, bool freezeAtEnd, float weight);
		static void AnimClip_Stop(int layerIndex = -1);
		static void AnimClip_Frame(Game::playerState_s* ps, int hand);
		static void WeaponRunXModelAnims_Stub();
		static void AddAnimClip();

		static void PM_Weapon_stub(Game::pmove_s* pm, Game::pml_t* pml);
	};
}

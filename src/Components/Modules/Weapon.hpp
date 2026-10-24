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
		static void CG_DrawItem_Stub();
		static void CG_CreateViewModelTree_Stub();

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

		// Per-player "no rechamber" (OMEN No Cock): the bolt never holds this
		// player back on the listed guns - changes their weapon state only,
		// never the shared weapon definition
		static void NoRechamber_BeforeWeapon(Game::pmove_s* pm);
		static void NoRechamber_AfterWeapon(Game::playerState_s* ps);
		static void AddNoRechamber();

		// Per-player fake lunge (OMEN test option): pressing melee with a
		// listed gun does the game's real melee-charge (lunge) instead of a
		// plain melee - without the pull toward a target
		static void FakeLunge_BeforeWeapon(Game::pmove_s* pm);
		static void FakeLunge_AfterWeapon(Game::playerState_s* ps);
		static void AddFakeLunge();

		// Per-player "server-only Sleight of Hand" (OMEN Slow Reload): the
		// player's game sees no SoH (Marathon reload animation) while the
		// server runs their weapon logic WITH it (SoH timing for everything)
		static bool ServerSoh_Begin(Game::playerState_s* ps, bool allowLocal);
		static void ServerSoh_End(Game::playerState_s* ps, bool applied);
		static void AddServerSoh();
		static void AddLocalModCommands();

		// Per-player Riot Shield Prone (OMEN)
		static bool RiotProne_For(int clientNum);
		static void RiotProne_SetPmClient(int clientNum);
		static bool BG_WeaponBlocksProne_Hk(unsigned int weapIndex);
		static bool BG_CanSwitchWhileProne_Hk(Game::playerState_s* ps, unsigned int weapIndex);
		static void AddRiotProne();

		// Viewmodel anim clip player
		static void AnimClip_Play(int layerIndex, unsigned int anim, float startPct, float endPct, float speed, bool freezeAtEnd, float weight,
			int mode = 0, int speedCurve = 0, int weightCurve = 0, int holdMs = 0, int blendMs = 0, int hands = 0);
		static void AnimClip_Stop(int layerIndex = -1);
		static void AnimClip_Frame(Game::playerState_s* ps, int hand);
		static void WeaponRunXModelAnims_Stub();
		static void AddAnimClip();

		static void PM_Weapon_stub(Game::pmove_s* pm, Game::pml_t* pml);
	};
}

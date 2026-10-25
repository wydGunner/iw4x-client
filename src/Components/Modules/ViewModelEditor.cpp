#include "ViewModelEditor.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"
#include "Weapon.hpp"

namespace Components
{
  // =====================================================================
  // OMEN Viewmodel Editor (2026-10-24)
  // ---------------------------------------------------------------------
  // Your first-person gun, on your own game only (nobody else ever draws
  // your first-person gun). Console commands - the OMEN menu sends these:
  //   vm_offset <forward> <right> <up>    move the gun (units)
  //   vm_tilt <pitch> <yaw> <roll>        turn the gun (degrees)
  //   vm_scale <size>                     1 = normal
  //   vm_parts                            the parts of what you're holding
  //   vm_hide <part> / vm_unhide <part>   hide a part (and what hangs on it)
  //   vm_hide_reset                       show every part again
  //   vm_skins [text]                     skins (materials) you can use
  //   vm_skin <material> / vm_skin reset  paint the gun you're holding
  //   vm_reset                            everything back
  //
  // Engine (iw4x.exe, found by disassembly):
  //   0x4AB212  CG_AddViewWeapon calls 0x59CA90, which fills the view
  //             weapon's GfxScaledPlacement at ebx (quat[4] +0x0, origin[3]
  //             +0x10, scale +0x1C - the game sets scale to 1.0) right
  //             before the gun is added to the scene. We adjust it after.
  //   0x9F5D28  per-hand view model info, stride 0x14, +0x0 DObj*
  //   DObj      numModels +0x9, hidePartBits[6] +0x80, models +0x98
  //   0x504F20  DObjGetBoneIndex(DObj*, scr_string name, uint8* index)
  //             (index starts at 0xFE = "look it up")
  // Hidden parts are set again every frame on the hand DObjs; we remember
  // which bits WE set, so showing a part again never un-hides one the game
  // hid itself (attachments' hideTags).
  // Skins change the gun model's material list (XModel::materialHandles) in
  // this game's memory - put back before every map change / disconnect.
  // Per-hand anim (more addresses):
  //   0x417542  call 0x50B9B0 = add a view model DObj to the scene
  //             (obj, placement, entnum 0x7FF / 0x7FE akimbo left, ...)
  //   0x428400  clear a DObj's skeleton (the game does it every frame)
  //   0x430780  ClientDObjCreate(models, count, tree, handle, lc)
  //   0x43FFF0  ClientDObjFree(handle); handles 0x800 / 0x801 = view model
  //   0x4CE030 / 0x4A03B0  create / free an anim tree
  // =====================================================================
  namespace
  {
    constexpr std::uintptr_t VIEWMODEL_HANDS = 0x9F5D28;
    constexpr std::uintptr_t VIEWMODEL_HAND_SIZE = 0x14;

    using DObjGetBoneIndex_t = int(*)(Game::DObj* obj, unsigned int name, unsigned char* index);
    const auto DObjGetBoneIndex = reinterpret_cast<DObjGetBoneIndex_t>(0x504F20);

    float offset[3]{};      // forward, right, up
    float tilt[3]{};        // pitch, yaw, roll (degrees)
    float scale = 1.0f;

    // per weapon index: part names to hide
    std::unordered_map<unsigned int, std::unordered_set<std::string>> hidden;
    // bits we OR'd in last frame, per hand
    unsigned int ourBits[2][6]{};

    // skins: original material lists per model we changed
    std::unordered_map<Game::XModel*, std::vector<Game::Material*>> savedSkins;

    DWORD stubReturn = 0;

    bool InGame()
    {
      return Game::CL_IsCgameInitialized() != 0;
    }

    unsigned int CurrentWeapon()
    {
      if (!InGame()) return 0;
      return static_cast<unsigned int>(Game::BG_GetViewModelWeaponIndex(&Game::cgArray[0].predictedPlayerState));
    }

    Game::DObj* HandDObj(const int hand)
    {
      return *reinterpret_cast<Game::DObj**>(VIEWMODEL_HANDS + hand * VIEWMODEL_HAND_SIZE);
    }

    // --- per-hand animation -------------------------------------------------
    // A second copy of your arms (its own DObj + anim tree, handle 0x801 -
    // the akimbo left gun's slot, unused on a normal gun) is drawn at the
    // same spot as the view model, playing its own anim. The picked arm is
    // hidden on the normal view model and only shown on the copy, so that
    // arm plays anim A while the other arm + gun keep playing the game's.
    struct DObjModel
    {
      Game::XModel* model;
      unsigned short boneName;
      bool ignoreCollision;
    };
    static_assert(sizeof(DObjModel) == 8);

    using ClientDObjCreate_t = Game::DObj*(*)(DObjModel* models, int numModels, Game::XAnimTree_s* tree, int handle, int localClientNum);
    const auto ClientDObjCreate = reinterpret_cast<ClientDObjCreate_t>(0x430780);
    using ClientDObjFree_t = void(*)(int handle);
    const auto ClientDObjFree = reinterpret_cast<ClientDObjFree_t>(0x43FFF0);
    using XAnimCreateSmallTree_t = Game::XAnimTree_s*(*)(void* anims);
    const auto XAnimCreateSmallTree = reinterpret_cast<XAnimCreateSmallTree_t>(0x4CE030);
    using XAnimFreeTree_t = void(*)(Game::XAnimTree_s* tree);
    const auto XAnimFreeTree = reinterpret_cast<XAnimFreeTree_t>(0x4A03B0);
    using XAnimSetGoalWeight_t = int(*)(Game::DObj* obj, unsigned int anim, float weight, float goalTime, float rate, unsigned int notifyName, unsigned int notifyType, int restart);
    const auto XAnimSetGoalWeight = reinterpret_cast<XAnimSetGoalWeight_t>(0x444B90);
    using XAnimSetTime_t = void(*)(Game::XAnimTree_s* tree, unsigned int anim, float time);
    const auto XAnimSetTime = reinterpret_cast<XAnimSetTime_t>(0x4DC640);
    using DObjSkelClear_t = void(*)(Game::DObj* obj);
    const auto DObjSkelClear = reinterpret_cast<DObjSkelClear_t>(0x428400); // the game does this per hand per frame
    using AddDObjToScene_t = void(*)(Game::DObj* obj, float* placement, int entnum, int flags, float* lightingOrigin, float materialTime, int extra);
    const auto AddDObjToScene = reinterpret_cast<AddDObjToScene_t>(0x50B9B0);

    constexpr int HANDANIM_HANDLE = 0x801;
    constexpr std::uintptr_t CLIENT_DOBJ_HANDLES = 0x1B288B0;  // short per handle -> pool slot
    constexpr std::uintptr_t CLIENT_DOBJ_POOL = 0x1ADA8A8;     // DObj pool, 0x9C each
    constexpr std::uintptr_t WEAPON_ANIM_SETS = 0x4EF57A;      // operand: per weapon XAnim_s* x3
    constexpr std::uintptr_t WEAPON_VIEWMODEL_CACHE = 0x7E3CC8; // per weapon, stride 0x20, +0x0 hands XModel*

    struct
    {
      Game::DObj* obj = nullptr;
      Game::XAnimTree_s* tree = nullptr;
      unsigned int weapon = 0;
      Game::XModel* hands = nullptr;
      bool drawn = false;       // drawn this frame (hide the arm on the real one)
      bool clip = false;        // Anim Clip drives it (Hand: Left Arm / Right Arm + Gun)
      int builtArm = -1;        // which arm the copy shows
      std::string why;          // last reason it isn't drawn (console)
      unsigned int realBits[6]{}; // bits we set on the real view model
    } handAnim;

    bool HandAnimObjAlive()
    {
      if (!handAnim.obj) return false;
      const auto slot = *reinterpret_cast<short*>(CLIENT_DOBJ_HANDLES + HANDANIM_HANDLE * 2);
      return slot && reinterpret_cast<Game::DObj*>(CLIENT_DOBJ_POOL + slot * 0x9C) == handAnim.obj;
    }

    // forget it without freeing (map change - the game clears its pools)
    void HandAnimDrop()
    {
      handAnim.obj = nullptr;
      handAnim.tree = nullptr;
      handAnim.weapon = 0;
      handAnim.hands = nullptr;
      handAnim.drawn = false;
      handAnim.builtArm = -1;
    }

    void HandAnimDestroy()
    {
      if (HandAnimObjAlive()) ClientDObjFree(HANDANIM_HANDLE);
      if (handAnim.tree) XAnimFreeTree(handAnim.tree);
      HandAnimDrop();
    }

    bool ArmBone(const char* name, const int arm)
    {
      if (!name) return false;
      const auto len = std::strlen(name);
      return len > 3 && std::strcmp(name + len - 3, arm == 0 ? "_le" : "_ri") == 0;
    }

    // hide every bone of one arm (or of the other one) on a DObj
    void SetArmBits(Game::DObj* obj, const Game::XModel* hands, const int arm, unsigned int* outBits)
    {
      if (!obj || !hands || !hands->boneNames) return;
      for (auto b = 0; b < hands->numBones; ++b)
      {
        const auto* name = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(hands->boneNames[b]));
        if (!ArmBone(name, arm)) continue;
        unsigned char index = 0xFE;
        if (!DObjGetBoneIndex(obj, hands->boneNames[b], &index) || index >= 192) continue;
        const auto bit = 0x80000000u >> (index & 31);
        if (obj->hidePartBits[index >> 5] & bit) continue;
        obj->hidePartBits[index >> 5] |= bit;
        if (outBits) outBits[index >> 5] |= bit;
      }
    }

    void ClearRealBits(Game::DObj* real)
    {
      if (real)
      {
        for (auto i = 0; i < 6; ++i) real->hidePartBits[i] &= ~handAnim.realBits[i];
      }
      std::memset(handAnim.realBits, 0, sizeof(handAnim.realBits));
    }

    void* WeaponAnims(const unsigned int weapon)
    {
      const auto base = *reinterpret_cast<std::uintptr_t*>(WEAPON_ANIM_SETS);
      if (!base) return nullptr;
      return *reinterpret_cast<void**>(base + weapon * 3 * sizeof(std::uintptr_t));
    }

    bool NotReady(const char* reason)
    {
      if (handAnim.why != reason)
      {
        handAnim.why = reason;
        Logger::Print("animclip arm: not drawn - {}\n", reason);
      }
      return false;
    }

    // build / keep the copy for the gun you're holding; false = don't draw it
    bool HandAnimReady()
    {
      handAnim.clip = Weapon::AnimClip_SplitActive();
      if (!handAnim.clip || !InGame()) return false;
      constexpr auto arm = 0; // the copy is the left arm

      // akimbo: that slot is the left gun's
      if (*reinterpret_cast<Game::DObj**>(VIEWMODEL_HANDS + VIEWMODEL_HAND_SIZE)) { HandAnimDrop(); return NotReady("akimbo gun"); }

      const auto weapon = CurrentWeapon();
      auto* hands = weapon ? *reinterpret_cast<Game::XModel**>(WEAPON_VIEWMODEL_CACHE + weapon * 0x20) : nullptr;
      auto* anims = weapon ? WeaponAnims(weapon) : nullptr;
      if (!weapon) return NotReady("no weapon");
      if (!hands) return NotReady("no hands model for this gun");
      if (!anims) return NotReady("no anims for this gun");

      if (handAnim.obj && (!HandAnimObjAlive() || handAnim.weapon != weapon || handAnim.hands != hands || handAnim.builtArm != arm))
      {
        HandAnimDestroy();
      }

      if (!handAnim.obj)
      {
        handAnim.tree = XAnimCreateSmallTree(anims);
        if (!handAnim.tree) return NotReady("couldn't make an anim tree");
        DObjModel model{ hands, 0, true };
        handAnim.obj = ClientDObjCreate(&model, 1, handAnim.tree, HANDANIM_HANDLE, 0);
        if (!handAnim.obj)
        {
          XAnimFreeTree(handAnim.tree);
          HandAnimDrop();
          return NotReady("couldn't make the arm copy");
        }
        handAnim.weapon = weapon;
        handAnim.hands = hands;
        // root blend node on, like the game does for the view model
        XAnimSetGoalWeight(handAnim.obj, 0, 1.0f, 0.0f, 1.0f, 0, 1, 0);
        // the copy only shows the picked arm
        SetArmBits(handAnim.obj, hands, arm == 0 ? 1 : 0, nullptr);
        handAnim.builtArm = arm;
        auto shown = 0;
        for (auto b = 0; b < hands->numBones; ++b)
        {
          if (ArmBone(Game::SL_ConvertToString(static_cast<Game::scr_string_t>(hands->boneNames[b])), arm)) ++shown;
        }
        Logger::Print("animclip arm: arm copy made for {} ({} bones, {} in the {} arm)\n", hands->name ? hands->name : "?", hands->numBones, shown, arm == 0 ? "left" : "right");
      }

      handAnim.why.clear();
      return true; // Anim Clip (Weapon.cpp) drives the copy's anims
    }

    // 0x417542: the view model hands are added to the scene here (entnum 0x7FF
    // = the gun you hold, 0x7FE = akimbo left gun). We add the copy after.
    void AddViewDObj_Hk(Game::DObj* obj, float* placement, const int entnum, const int flags, float* lightingOrigin, const float materialTime, const int extra)
    {
      if (entnum == 0x7FF)
      {
        ClearRealBits(obj);
        handAnim.drawn = HandAnimReady();
        if (handAnim.drawn)
        {
          SetArmBits(obj, handAnim.hands, handAnim.builtArm, handAnim.realBits);
        }
      }

      AddDObjToScene(obj, placement, entnum, flags, lightingOrigin, materialTime, extra);

      if (entnum == 0x7FF && handAnim.drawn)
      {
        DObjSkelClear(handAnim.obj);
        AddDObjToScene(handAnim.obj, placement, 0x7FE, flags, lightingOrigin, materialTime, extra);
      }
    }

    // 0x59C196: the game builds a view model DObj. If it's about to use our
    // slot (switching to akimbo) free ours first so nothing leaks.
    Game::DObj* ViewDObjCreate_Hk(DObjModel* models, const int numModels, Game::XAnimTree_s* tree, const int handle, const int localClientNum)
    {
      if (handle == HANDANIM_HANDLE && handAnim.obj)
      {
        HandAnimDestroy();
      }
      return ClientDObjCreate(models, numModels, tree, handle, localClientNum);
    }

    // --- quaternion helpers (x, y, z, w) --------------------------------
    void QuatMul(const float* a, const float* b, float* out)
    {
      const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
      const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
      const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
      const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
      out[0] = x; out[1] = y; out[2] = z; out[3] = w;
    }

    void QuatRotate(const float* q, const float* v, float* out)
    {
      const float p[4] = { v[0], v[1], v[2], 0.0f };
      const float c[4] = { -q[0], -q[1], -q[2], q[3] };
      float t[4], r[4];
      QuatMul(q, p, t);
      QuatMul(t, c, r);
      out[0] = r[0]; out[1] = r[1]; out[2] = r[2];
    }

    void QuatFromAngles(const float pitch, const float yaw, const float roll, float* out)
    {
      constexpr float d2r = 3.14159265f / 180.0f;
      const float qy[4] = { 0.0f, 0.0f, std::sin(yaw * d2r * 0.5f), std::cos(yaw * d2r * 0.5f) };
      const float qp[4] = { 0.0f, std::sin(pitch * d2r * 0.5f), 0.0f, std::cos(pitch * d2r * 0.5f) };
      const float qr[4] = { std::sin(roll * d2r * 0.5f), 0.0f, 0.0f, std::cos(roll * d2r * 0.5f) };
      float t[4];
      QuatMul(qy, qp, t);
      QuatMul(t, qr, out);
    }

    // --- hidden parts -----------------------------------------------------
    // a bone of a model, plus its parent chain, by name
    bool AncestorHidden(const Game::XModel* model, int bone, const std::unordered_set<std::string>& names)
    {
      for (auto guard = 0; bone >= 0 && guard < 256; ++guard)
      {
        const auto* name = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(model->boneNames[bone]));
        if (name && names.contains(Utils::String::ToLower(name))) return true;
        if (bone < model->numRootBones) break;
        bone -= model->parentList[bone - model->numRootBones];
      }
      return false;
    }

    void ApplyHidden(const int hand, const std::unordered_set<std::string>* names)
    {
      auto* obj = HandDObj(hand);
      if (!obj) return;

      // take back what we set last frame
      for (auto i = 0; i < 6; ++i)
      {
        obj->hidePartBits[i] &= ~ourBits[hand][i];
        ourBits[hand][i] = 0;
      }
      if (!names || names->empty() || !obj->models) return;

      for (auto m = 0; m < obj->numModels; ++m)
      {
        const auto* model = obj->models[m];
        if (!model || !model->boneNames) continue;
        for (auto b = 0; b < model->numBones; ++b)
        {
          if (!AncestorHidden(model, b, *names)) continue;
          unsigned char index = 0xFE;
          if (!DObjGetBoneIndex(obj, model->boneNames[b], &index) || index >= 192) continue;
          const auto bit = 0x80000000u >> (index & 31);
          if (obj->hidePartBits[index >> 5] & bit) continue; // the game hid it already
          obj->hidePartBits[index >> 5] |= bit;
          ourBits[hand][index >> 5] |= bit;
        }
      }
    }

    // --- skins --------------------------------------------------------------
    std::vector<Game::XModel*> GunModels(const unsigned int weapIndex)
    {
      std::vector<Game::XModel*> out;
      const auto* def = weapIndex ? Game::BG_GetWeaponDef(weapIndex) : nullptr;
      if (!def || !def->gunXModel) return out;
      for (auto i = 0; i < 16; ++i)
      {
        auto* m = def->gunXModel[i];
        if (m && m->materialHandles && std::find(out.begin(), out.end(), m) == out.end()) out.push_back(m);
      }
      return out;
    }

    const char* MaterialName(const Game::Material* mat)
    {
      return (mat && mat->info.name) ? mat->info.name : "";
    }
  }

  Game::DObj* ViewModelEditor::SplitCopy()
  {
    return (handAnim.clip && HandAnimObjAlive()) ? handAnim.obj : nullptr;
  }

  void ViewModelEditor::RestoreAll()
  {
    for (auto& [model, mats] : savedSkins)
    {
      if (!model || !model->materialHandles) continue;
      for (std::size_t i = 0; i < mats.size() && i < model->numsurfs; ++i)
      {
        model->materialHandles[i] = mats[i];
      }
    }
    savedSkins.clear();
  }

  void ViewModelEditor::AfterWeaponPosition(float* placement)
  {
    if (!placement) return;

    // position (along the gun's own forward / right / up)
    if (offset[0] != 0.0f || offset[1] != 0.0f || offset[2] != 0.0f)
    {
      const float local[3] = { offset[0], -offset[1], offset[2] };
      float world[3];
      QuatRotate(placement, local, world);
      placement[4] += world[0];
      placement[5] += world[1];
      placement[6] += world[2];
    }

    // tilt
    if (tilt[0] != 0.0f || tilt[1] != 0.0f || tilt[2] != 0.0f)
    {
      float t[4], q[4];
      QuatFromAngles(tilt[0], tilt[1], tilt[2], t);
      QuatMul(placement, t, q);
      std::memcpy(placement, q, sizeof(q));
    }

    // size
    if (scale != 1.0f)
    {
      placement[7] *= scale;
    }

    // hidden parts on both hands
    const auto weapIndex = CurrentWeapon();
    const auto it = hidden.find(weapIndex);
    const auto* names = it == hidden.end() ? nullptr : &it->second;
    ApplyHidden(0, names);
    ApplyHidden(1, names);
  }

  __declspec(naked) void ViewModelEditor::CalcWeaponPosition_Stub()
  {
    __asm
    {
      pop stubReturn

      mov edx, 0x59CA90 // CG_CalculateWeaponPosition-ish (fills the placement at ebx)
      call edx

      pushad
      push ebx
      call AfterWeaponPosition
      add esp, 4
      popad

      push stubReturn
      ret
    }
  }

  ViewModelEditor::ViewModelEditor()
  {
    // gsc (server): GetGunParts(<weapon>) - part (bone) names of that gun's
    // first-person model, for the menu's Hide Part list
    GSC::Script::AddFunction("GetGunParts", []
    {
      Game::Scr_MakeArray();
      if (Game::Scr_GetNumParam() < 1) return;
      const auto weapIndex = Game::G_GetWeaponIndexForName(Game::Scr_GetString(0));
      const auto models = GunModels(weapIndex);
      if (models.empty()) return;
      const auto* model = models[0];
      for (auto b = 0; b < model->numBones; ++b)
      {
        const auto* name = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(model->boneNames[b]));
        if (!name || !*name) continue;
        Game::Scr_AddString(name);
        Game::Scr_AddArray();
      }
    });

    // gsc (server): GetGunSkins() - materials of every loaded weapon's
    // first-person model, for the menu's Skin list
    GSC::Script::AddFunction("GetGunSkins", []
    {
      Game::Scr_MakeArray();
      std::set<std::string> names;
      const auto count = Game::BG_GetNumWeapons();
      for (auto w = 1u; w < count; ++w)
      {
        for (auto* model : GunModels(w))
        {
          for (auto i = 0; i < model->numsurfs; ++i) names.insert(MaterialName(model->materialHandles[i]));
        }
      }
      for (const auto& n : names)
      {
        if (n.empty()) continue;
        Game::Scr_AddString(n.data());
        Game::Scr_AddArray();
      }
    });

    if (Dedicated::IsEnabled()) return;

    Utils::Hook(0x4AB212, CalcWeaponPosition_Stub, HOOK_CALL).install()->quick();
    Utils::Hook(0x417542, AddViewDObj_Hk, HOOK_CALL).install()->quick();
    Utils::Hook(0x59C196, ViewDObjCreate_Hk, HOOK_CALL).install()->quick();

    const auto floats = [](const Command::Params* params, float* out, const int count)
    {
      for (auto i = 0; i < count; ++i)
      {
        out[i] = params->size() > i + 1 ? static_cast<float>(std::atof(params->get(i + 1))) : 0.0f;
      }
    };

    Command::Add("vm_offset", [floats](const Command::Params* params) { floats(params, offset, 3); });
    Command::Add("vm_tilt", [floats](const Command::Params* params) { floats(params, tilt, 3); });
    Command::Add("vm_scale", [](const Command::Params* params)
    {
      const auto s = params->size() > 1 ? static_cast<float>(std::atof(params->get(1))) : 1.0f;
      scale = (s > 0.05f && s < 20.0f) ? s : 1.0f;
    });

    Command::Add("vm_parts", []
    {
      auto* obj = HandDObj(0);
      if (!InGame() || !obj || !obj->models)
      {
        Logger::Print("vm_parts: hold a gun in game first\n");
        return;
      }
      for (auto m = 0; m < obj->numModels; ++m)
      {
        const auto* model = obj->models[m];
        if (!model) continue;
        Logger::Print("{}:\n", model->name ? model->name : "?");
        for (auto b = 0; b < model->numBones; ++b)
        {
          const auto* name = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(model->boneNames[b]));
          Logger::Print("  {}\n", name ? name : "?");
        }
      }
    });

    Command::Add("vm_hide", [](const Command::Params* params)
    {
      const auto weapIndex = CurrentWeapon();
      if (!weapIndex || params->size() < 2) return;
      hidden[weapIndex].insert(Utils::String::ToLower(params->get(1)));
    });

    Command::Add("vm_unhide", [](const Command::Params* params)
    {
      const auto weapIndex = CurrentWeapon();
      if (!weapIndex || params->size() < 2) return;
      hidden[weapIndex].erase(Utils::String::ToLower(params->get(1)));
    });

    Command::Add("vm_hide_reset", []
    {
      hidden.erase(CurrentWeapon());
    });

    Command::Add("vm_skins", [](const Command::Params* params)
    {
      const std::string filter = params->size() > 1 ? Utils::String::ToLower(params->get(1)) : "";
      std::set<std::string> names;
      const auto count = Game::BG_GetNumWeapons();
      for (auto w = 1u; w < count; ++w)
      {
        for (auto* model : GunModels(w))
        {
          for (auto s = 0; s < model->numsurfs; ++s) names.insert(MaterialName(model->materialHandles[s]));
        }
      }
      for (const auto& n : names)
      {
        if (n.empty() || (!filter.empty() && Utils::String::ToLower(n).find(filter) == std::string::npos)) continue;
        Logger::Print("  {}\n", n);
      }
    });

    Command::Add("vm_skin", [](const Command::Params* params)
    {
      if (params->size() < 2) return;
      const std::string name = params->get(1);
      const auto models = GunModels(CurrentWeapon());

      if (name == "reset")
      {
        for (auto* model : models)
        {
          const auto it = savedSkins.find(model);
          if (it == savedSkins.end()) continue;
          for (std::size_t i = 0; i < it->second.size() && i < model->numsurfs; ++i) model->materialHandles[i] = it->second[i];
          savedSkins.erase(it);
        }
        return;
      }

      if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_MATERIAL, name.data()))
      {
        Logger::Print("vm_skin: '{}' isn't loaded (vm_skins shows what is)\n", name);
        return;
      }
      auto* mat = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, name.data()).material;
      if (!mat) return;

      for (auto* model : models)
      {
        if (!savedSkins.contains(model))
        {
          savedSkins[model] = std::vector<Game::Material*>(model->materialHandles, model->materialHandles + model->numsurfs);
        }
        for (auto s = 0; s < model->numsurfs; ++s) model->materialHandles[s] = mat;
      }
    });

    Command::Add("vm_reset", []
    {
      offset[0] = offset[1] = offset[2] = 0.0f;
      tilt[0] = tilt[1] = tilt[2] = 0.0f;
      scale = 1.0f;
      hidden.clear();
      RestoreAll();
    });

    // weapon indices / models belong to the map
    Events::OnCLDisconnected([](bool) { RestoreAll(); hidden.clear(); HandAnimDrop(); std::memset(handAnim.realBits, 0, sizeof(handAnim.realBits)); });
    Events::OnCGameInit([] { RestoreAll(); hidden.clear(); HandAnimDrop(); std::memset(handAnim.realBits, 0, sizeof(handAnim.realBits)); });
  }
}

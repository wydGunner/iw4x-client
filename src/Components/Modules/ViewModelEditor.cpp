#include "ViewModelEditor.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"

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
  //   vm_skin <material> [where]          paint the gun you're holding
  //       where: gun (default) / arms / a part (bone) of the gun or arms
  //   vm_skin reset                       gun + arms back
  //   vm_reset                            everything back
  //   flip <back|front|left|right> [ms]   your view does a full flip (only you)
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

    // --- view flip ------------------------------------------------------------
    // 0x486D37 calls CG_AddViewWeapon(lc) (0x4AB140) once the view for this frame
    // is worked out (cg.refdefViewAngles 0x85F650, cg.refdef.view.axis 0x85B714,
    // the game rebuilds both every frame). We turn both a bit further before the
    // gun is placed, so the gun turns with the view and the frame is drawn flipped.
    constexpr std::uintptr_t REFDEF_VIEW_ANGLES = 0x85F650;
    constexpr std::uintptr_t REFDEF_VIEW_AXIS = 0x85B714;
    using AddViewWeapon_t = void(*)(int localClientNum);
    const auto AddViewWeapon = reinterpret_cast<AddViewWeapon_t>(0x4AB140);

    struct
    {
      bool active = false;
      int type = 0;      // 0 back, 1 front, 2 left, 3 right
      int start = 0;     // cg.time
      int ms = 600;
    } flip;

    // v about unit axis k by a radians
    void RotateAbout(float* v, const float* k, const float a)
    {
      const auto c = std::cos(a), sn = std::sin(a);
      const float cross[3] = { k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0] };
      const auto dot = k[0] * v[0] + k[1] * v[1] + k[2] * v[2];
      for (auto i = 0; i < 3; ++i) v[i] = v[i] * c + cross[i] * sn + k[i] * dot * (1.0f - c);
    }

    void AddViewWeapon_Hk(const int localClientNum)
    {
      if (flip.active && Game::CL_IsCgameInitialized())
      {
        const auto now = Game::cgArray[0].time;
        auto u = static_cast<float>(now - flip.start) / static_cast<float>(std::max(flip.ms, 1));
        if (u >= 1.0f || u < 0.0f)
        {
          flip.active = false;
        }
        else
        {
          const auto ease = u * u * (3.0f - 2.0f * u);           // slow - fast - slow
          const auto deg = 360.0f * ease;
          auto* angles = reinterpret_cast<float*>(REFDEF_VIEW_ANGLES);
          auto* axis = reinterpret_cast<float(*)[3]>(REFDEF_VIEW_AXIS);
          float k[3];
          float a = 0.0f;
          switch (flip.type)
          {
          case 0: // backflip: looking up and over (pitch going negative), about the left axis
            angles[0] -= deg; a = -deg; std::memcpy(k, axis[1], sizeof(k)); break;
          case 1: // frontflip
            angles[0] += deg; a = deg; std::memcpy(k, axis[1], sizeof(k)); break;
          case 2: // left: roll to the left, about the forward axis
            angles[2] -= deg; a = -deg; std::memcpy(k, axis[0], sizeof(k)); break;
          default: // right
            angles[2] += deg; a = deg; std::memcpy(k, axis[0], sizeof(k)); break;
          }
          const auto rad = a * 3.14159265f / 180.0f;
          for (auto i = 0; i < 3; ++i) RotateAbout(axis[i], k, rad);
        }
      }
      AddViewWeapon(localClientNum);
    }

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

    // the arms model of the view model you're holding (DObj model 0)
    Game::XModel* HandsModel()
    {
      auto* obj = HandDObj(0);
      return (obj && obj->models && obj->numModels > 0) ? obj->models[0] : nullptr;
    }

    int BoneIndexOf(const Game::XModel* model, const std::string& name)
    {
      if (!model || !model->boneNames) return -1;
      for (auto b = 0; b < model->numBones; ++b)
      {
        const auto* n = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(model->boneNames[b]));
        if (n && Utils::String::ToLower(n) == name) return b;
      }
      return -1;
    }

    // bone b is root or hangs under it
    bool InSubtree(const Game::XModel* model, int b, const int root)
    {
      for (auto guard = 0; b >= 0 && guard < 256; ++guard)
      {
        if (b == root) return true;
        if (b < model->numRootBones) return false;
        b -= model->parentList[b - model->numRootBones];
      }
      return false;
    }

    // Surfaces (material slots) of a model that make up one part: the ones
    // that only use that part's bones; if none do, the ones that touch it.
    // root < 0 = every surface.
    std::vector<int> PartSurfaces(const Game::XModel* model, const int root)
    {
      std::vector<int> only, touch;
      if (!model || !model->materialHandles) return only;
      for (auto lod = 0; lod < 4 && lod < std::max<int>(model->numLods, 1); ++lod)
      {
        const auto& info = model->lodInfo[lod];
        if (!info.surfs) continue;
        for (auto i = 0; i < info.numsurfs; ++i)
        {
          const auto slot = info.surfIndex + i;
          if (slot >= model->numsurfs) continue;
          if (root < 0) { only.push_back(slot); continue; }
          auto in = false, out = false;
          for (auto b = 0; b < model->numBones && b < 192; ++b)
          {
            if (!(info.surfs[i].partBits[b >> 5] & (0x80000000u >> (b & 31)))) continue;
            if (InSubtree(model, b, root)) in = true; else out = true;
          }
          if (in && !out) only.push_back(slot);
          if (in) touch.push_back(slot);
        }
      }
      if (root < 0 && only.empty())
      {
        for (auto slot = 0; slot < model->numsurfs; ++slot) only.push_back(slot);
      }
      return only.empty() ? touch : only;
    }

    void PaintSurfaces(Game::XModel* model, const std::vector<int>& slots, Game::Material* mat)
    {
      if (!model || !model->materialHandles || slots.empty()) return;
      if (!savedSkins.contains(model))
      {
        savedSkins[model] = std::vector<Game::Material*>(model->materialHandles, model->materialHandles + model->numsurfs);
      }
      for (const auto slot : slots) model->materialHandles[slot] = mat;
    }

    void UnpaintModel(Game::XModel* model)
    {
      const auto it = savedSkins.find(model);
      if (it == savedSkins.end() || !model || !model->materialHandles) return;
      for (std::size_t i = 0; i < it->second.size() && i < model->numsurfs; ++i) model->materialHandles[i] = it->second[i];
      savedSkins.erase(it);
    }
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
    Utils::Hook(0x486D37, AddViewWeapon_Hk, HOOK_CALL).install()->quick();

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
      const std::string where = params->size() > 2 ? Utils::String::ToLower(params->get(2)) : "gun";
      const auto models = GunModels(CurrentWeapon());
      auto* hands = HandsModel();

      if (name == "reset")
      {
        for (auto* model : models) UnpaintModel(model);
        UnpaintModel(hands);
        return;
      }

      if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_MATERIAL, name.data()))
      {
        Logger::Print("vm_skin: '{}' isn't loaded (vm_skins shows what is)\n", name);
        return;
      }
      auto* mat = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, name.data()).material;
      if (!mat) return;

      if (where == "gun" || where == "whole" || where.empty())
      {
        for (auto* model : models) PaintSurfaces(model, PartSurfaces(model, -1), mat);
        return;
      }
      if (where == "arms" || where == "hands")
      {
        PaintSurfaces(hands, PartSurfaces(hands, -1), mat);
        return;
      }

      // one part: a bone of the gun (every camo version of it) or of the arms
      auto painted = 0;
      for (auto* model : models)
      {
        const auto bone = BoneIndexOf(model, where);
        if (bone < 0) continue;
        const auto slots = PartSurfaces(model, bone);
        PaintSurfaces(model, slots, mat);
        painted += static_cast<int>(slots.size());
      }
      if (!painted && hands)
      {
        const auto bone = BoneIndexOf(hands, where);
        if (bone >= 0)
        {
          const auto slots = PartSurfaces(hands, bone);
          PaintSurfaces(hands, slots, mat);
          painted = static_cast<int>(slots.size());
        }
      }
      if (!painted) Logger::Print("vm_skin: nothing to paint on '{}' (vm_parts shows the parts)\n", where);
    });

    Command::Add("flip", [](const Command::Params* params)
    {
      const std::string kind = params->size() > 1 ? Utils::String::ToLower(params->get(1)) : "back";
      if (kind == "front") flip.type = 1;
      else if (kind == "left") flip.type = 2;
      else if (kind == "right") flip.type = 3;
      else flip.type = 0;
      flip.ms = params->size() > 2 ? std::clamp(std::atoi(params->get(2)), 150, 5000) : 600;
      if (!Game::CL_IsCgameInitialized()) return;
      flip.start = Game::cgArray[0].time;
      flip.active = true;
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
    Events::OnCLDisconnected([](bool) { RestoreAll(); hidden.clear(); });
    Events::OnCGameInit([] { RestoreAll(); hidden.clear(); });
  }
}

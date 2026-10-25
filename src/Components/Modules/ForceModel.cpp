#include "ForceModel.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"

namespace Components
{
  // =====================================================================
  // OMEN forcemodel (2026-10-24)
  // ---------------------------------------------------------------------
  // First person (your own game, needs this dll):
  //   forcemodel <xmodel>      the gun you're holding shows that model
  //   forcemodel reset         every gun back to its own model
  //   forcemodel_list [text]   loaded models (optionally only names with text)
  // It swaps the gun's first-person model entries in YOUR game's copy of the
  // weapon (gunXModel). Nobody else sees it - other players never draw your
  // first-person gun. On a listen server the server never uses gunXModel.
  // Shows the next time the game rebuilds the view model (switch weapons /
  // pull the gun out again if it doesn't change right away).
  //
  // A forced model may live in the current map's files, so every swap is put
  // back BEFORE the map changes (map change announcement, disconnect) - after
  // that the old pointers could be gone.
  //
  // Player model (everyone sees it) is done in GSC with setModel; the menu
  // gets its list from GetLoadedModels() (the map's precached models, the only
  // ones setModel can use) and the first-person list from GetViewModels().
  // =====================================================================
  namespace
  {
    constexpr int GUN_MODEL_SLOTS = 16; // camo variants

    struct Saved
    {
      Game::WeaponDef* def;
      Game::XModel* models[GUN_MODEL_SLOTS];
    };

    std::unordered_map<unsigned int, Saved> saved; // weapon index -> original models

    bool HaveCgame()
    {
      return Game::CL_IsCgameInitialized() != 0;
    }

    Game::XModel* FindModel(const char* name)
    {
      if (!name || !*name) return nullptr;
      if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XMODEL, name)) return nullptr;
      return Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, name).model;
    }

    void RestoreOne(const unsigned int weapIndex, const Saved& s)
    {
      // only if the game still has the same weapon at the same place
      if (Game::BG_GetWeaponDef(weapIndex) != s.def || !s.def->gunXModel) return;
      for (auto i = 0; i < GUN_MODEL_SLOTS; ++i)
      {
        s.def->gunXModel[i] = s.models[i];
      }
    }
  }

  void ForceModel::RestoreAll()
  {
    for (const auto& [weapIndex, s] : saved)
    {
      RestoreOne(weapIndex, s);
    }
    saved.clear();
  }

  ForceModel::ForceModel()
  {
    Command::Add("forcemodel", [](const Command::Params* params)
    {
      if (params->size() < 2)
      {
        Logger::Print("forcemodel <xmodel>  /  forcemodel reset  /  forcemodel_list [text]\n");
        return;
      }

      const std::string name = params->get(1);
      if (name == "reset")
      {
        RestoreAll();
        Logger::Print("forcemodel: every gun back to its own model\n");
        return;
      }

      if (!HaveCgame())
      {
        Logger::Print("forcemodel: only in game\n");
        return;
      }

      auto* model = FindModel(name.data());
      if (!model)
      {
        Logger::Print("forcemodel: '{}' isn't loaded on this map (forcemodel_list shows what is)\n", name);
        return;
      }

      const auto weapIndex = static_cast<unsigned int>(Game::BG_GetViewModelWeaponIndex(&Game::cgArray[0].predictedPlayerState));
      auto* def = weapIndex ? Game::BG_GetWeaponDef(weapIndex) : nullptr;
      if (!def || !def->gunXModel)
      {
        Logger::Print("forcemodel: hold a gun first\n");
        return;
      }

      if (!saved.contains(weapIndex))
      {
        Saved s{};
        s.def = def;
        for (auto i = 0; i < GUN_MODEL_SLOTS; ++i) s.models[i] = def->gunXModel[i];
        saved[weapIndex] = s;
      }

      // every camo slot the gun uses
      for (auto i = 0; i < GUN_MODEL_SLOTS; ++i)
      {
        if (def->gunXModel[i]) def->gunXModel[i] = model;
      }

      Logger::Print("forcemodel: first-person model of this gun is now {} (switch weapons if it didn't change yet)\n", name);
    });

    Command::Add("forcemodel_list", [](const Command::Params* params)
    {
      std::string filter = params->size() > 1 ? Utils::String::ToLower(params->get(1)) : "";
      std::vector<std::string> names;

      Game::DB_EnumXAssets(Game::ASSET_TYPE_XMODEL, [](Game::XAssetHeader header, void* data)
      {
        auto* list = static_cast<std::vector<std::string>*>(data);
        if (header.model && header.model->name) list->emplace_back(header.model->name);
      }, &names, false);

      std::sort(names.begin(), names.end());
      auto shown = 0;
      for (const auto& n : names)
      {
        if (!filter.empty() && Utils::String::ToLower(n).find(filter) == std::string::npos) continue;
        Logger::Print("  {}\n", n);
        ++shown;
      }
      Logger::Print("forcemodel_list: {} models\n", shown);
    });

    // before the old map's files go away
    Events::OnCLDisconnected([](bool) { RestoreAll(); });
    Events::OnCGameInit([] { RestoreAll(); });

    // gsc: GetLoadedModels() - the map's precached models (what setModel can use)
    GSC::Script::AddFunction("GetLoadedModels", []
    {
      Game::Scr_MakeArray();
      for (int i = Game::CS_MODELS; i <= static_cast<int>(Game::CS_MODELS_LAST); ++i)
      {
        const auto id = Game::SV_GetConfigstringConst(i);
        if (!id) continue;
        const auto* name = Game::SL_ConvertToString(static_cast<Game::scr_string_t>(id));
        if (!name || !*name || *name == '*') continue; // brush models
        Game::Scr_AddString(name);
        Game::Scr_AddArray();
      }
    });

    // gsc: GetViewModels() - first-person gun models of the loaded weapons
    GSC::Script::AddFunction("GetViewModels", []
    {
      Game::Scr_MakeArray();
      std::unordered_set<std::string> seen;
      const auto count = Game::BG_GetNumWeapons();
      for (auto w = 1u; w < count; ++w)
      {
        const auto* def = Game::BG_GetWeaponDef(w);
        if (!def || !def->gunXModel || !def->gunXModel[0] || !def->gunXModel[0]->name) continue;
        const std::string name = def->gunXModel[0]->name;
        if (!seen.insert(name).second) continue;
        Game::Scr_AddString(name.data());
        Game::Scr_AddArray();
      }
    });
  }
}

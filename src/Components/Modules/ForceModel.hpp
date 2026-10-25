#pragma once

namespace Components
{
  // OMEN forcemodel: your first-person gun model (own game, console command)
  // and lists of loaded models for the menu (player model, GSC)
  class ForceModel : public Component
  {
  public:
    ForceModel();

    // put every forced first-person model back (before a map change / disconnect)
    static void RestoreAll();
  };
}

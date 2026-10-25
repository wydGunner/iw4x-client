#pragma once

namespace Components
{
  // OMEN Viewmodel Editor: your first-person gun - position / tilt / size,
  // hidden parts, skin. Your own game only (needs this dll).
  class ViewModelEditor : public Component
  {
  public:
    ViewModelEditor();

    // skins back before a map change / disconnect (model pointers go away)
    static void RestoreAll();

  private:
    static void CalcWeaponPosition_Stub();
    static void AfterWeaponPosition(float* placement);
  };
}

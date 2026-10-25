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

    // the left-arm copy while an Anim Clip layer is on Left Arm / Right Arm + Gun
    // (Weapon.cpp plays the clip on it), else nullptr
    static Game::DObj* SplitCopy();

  private:
    static void CalcWeaponPosition_Stub();
    static void AfterWeaponPosition(float* placement);
  };
}

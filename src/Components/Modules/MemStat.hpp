#pragma once

namespace Components
{
  // OMEN: script memory report (MT_AllocIndex "failed allocation ... for script usage")
  class MemStat : public Component
  {
  public:
    MemStat();

    static void Report(const char* when, bool full);

  private:
    static void MT_AllocFail_Hk(int code, const char* fmt, const char* who, int size);
  };
}

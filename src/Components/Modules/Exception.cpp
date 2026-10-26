#include "Exception.hpp"

namespace Components
{
  Utils::Hook Exception::SetFilterHook;
  LPTOP_LEVEL_EXCEPTION_FILTER Exception::ChainedFilter;

  __declspec(noreturn) void Exception::LongJmp_Internal_Stub(jmp_buf env, int status)
  {
    AssetHandler::ResetBypassState();
    Game::longjmp_internal(env, status);
  }

  void Exception::WriteOmenCrashReport(LPEXCEPTION_POINTERS ExceptionInfo)
  {
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1) != 0)
      return;

    SYSTEMTIME t{};
    GetLocalTime(&t);
    char stamp[64];
    sprintf_s(stamp, "%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    CreateDirectoryA("minidumps", nullptr);

    char dumpPath[MAX_PATH];
    sprintf_s(dumpPath, "minidumps\\omen-%s.dmp", stamp);
    const auto file = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
      MINIDUMP_EXCEPTION_INFORMATION mei{};
      mei.ThreadId = GetCurrentThreadId();
      mei.ExceptionPointers = ExceptionInfo;
      mei.ClientPointers = FALSE;
      MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
        static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
        &mei, nullptr, nullptr);
      CloseHandle(file);
    }

    // text report: exception code, address, and which module + offset it is in
    const auto* rec = ExceptionInfo->ExceptionRecord;
    const auto addr = reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress);
    char modName[MAX_PATH] = "?";
    std::uintptr_t modBase = 0;
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCSTR>(rec->ExceptionAddress), &mod) && mod)
    {
      GetModuleFileNameA(mod, modName, sizeof(modName));
      modBase = reinterpret_cast<std::uintptr_t>(mod);
    }

    char txtPath[MAX_PATH];
    sprintf_s(txtPath, "minidumps\\omen-%s.txt", stamp);
    FILE* fp = nullptr;
    if (fopen_s(&fp, txtPath, "w") == 0 && fp)
    {
      fprintf(fp, "OMEN crash %s\ncode 0x%08lX at 0x%08X\nmodule %s base 0x%08X offset 0x%08X\n",
        stamp, rec->ExceptionCode, static_cast<unsigned>(addr), modName, static_cast<unsigned>(modBase),
        static_cast<unsigned>(addr - modBase));
      if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
        fprintf(fp, "access violation: %s 0x%08X\n", rec->ExceptionInformation[0] ? "write to" : "read from",
          static_cast<unsigned>(rec->ExceptionInformation[1]));
      const auto* ctx = ExceptionInfo->ContextRecord;
      fprintf(fp, "eip 0x%08lX esp 0x%08lX ebp 0x%08lX eax 0x%08lX ebx 0x%08lX ecx 0x%08lX edx 0x%08lX esi 0x%08lX edi 0x%08lX\n",
        ctx->Eip, ctx->Esp, ctx->Ebp, ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx, ctx->Esi, ctx->Edi);
      // raw stack words (return addresses show up here)
      const auto* sp = reinterpret_cast<const std::uint32_t*>(ctx->Esp);
      fprintf(fp, "stack:");
      for (int i = 0; i < 64; ++i)
      {
        if (IsBadReadPtr(sp + i, sizeof(std::uint32_t)))
          break;
        fprintf(fp, "%s%08X", (i % 8) ? " " : "\n  ", sp[i]);
      }
      fprintf(fp, "\n");
      fclose(fp);
    }
  }

  LONG WINAPI Exception::ExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo)
  {
    // Pass on harmless errors
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_INTEGER_OVERFLOW ||
      ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_FLOAT_OVERFLOW)
    {
      return EXCEPTION_CONTINUE_EXECUTION;
    }

    // OMEN (2026-10-26): write our own crash dump + a one-line text report
    // first. The newer IW4x crash reporter (chained below) has no DSN in our
    // builds, so it swallowed crashes without leaving anything behind.
    WriteOmenCrashReport(ExceptionInfo);

    if (ChainedFilter)
    {
      ChainedFilter(ExceptionInfo);
    }

    TerminateProcess(GetCurrentProcess(), ExceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_CONTINUE_SEARCH;
  }

  void Exception::ChainFilterInstalledBy(const std::function<void()>& installer)
  {
    SetFilterHook.uninstall();

    installer();

    const auto chained = ::SetUnhandledExceptionFilter(&ExceptionFilter);
    if (chained != &ExceptionFilter)
    {
      ChainedFilter = chained;
    }

    SetFilterHook.install();
  }

  LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Exception::SetUnhandledExceptionFilter_Stub(LPTOP_LEVEL_EXCEPTION_FILTER)
  {
    SetFilterHook.uninstall();
    LPTOP_LEVEL_EXCEPTION_FILTER result = ::SetUnhandledExceptionFilter(&ExceptionFilter);
    SetFilterHook.install();
    return result;
  }

  Exception::Exception()
  {
    SetFilterHook.initialize(::SetUnhandledExceptionFilter, SetUnhandledExceptionFilter_Stub, HOOK_JUMP);
    SetFilterHook.install();

    ::SetUnhandledExceptionFilter(&ExceptionFilter);

    Utils::Hook(0x4B241F, LongJmp_Internal_Stub, HOOK_CALL).install()->quick();
    Utils::Hook(0x61DB44, LongJmp_Internal_Stub, HOOK_CALL).install()->quick();
    Utils::Hook(0x61F17D, LongJmp_Internal_Stub, HOOK_CALL).install()->quick();
    Utils::Hook(0x61F248, LongJmp_Internal_Stub, HOOK_CALL).install()->quick();
    Utils::Hook(0x61F5E7, LongJmp_Internal_Stub, HOOK_CALL).install()->quick();
  }

  Exception::~Exception()
  {
    SetFilterHook.uninstall();
  }
}

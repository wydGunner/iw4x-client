#include "MemStat.hpp"
#include "Events.hpp"

namespace Components
{
  // =====================================================================
  // OMEN script memory report (2026-10-24)
  // ---------------------------------------------------------------------
  // The game keeps every script string (and other script data, like the
  // saved stacks of waiting script threads) in one fixed pool: 65536 slots
  // of 12 bytes (768 KB). When it's full - or too chopped up for a big
  // piece - the game stops with "MT_AllocIndex: failed allocation of N
  // bytes for script usage". In a private match the match and your own game
  // share that pool.
  //
  // This only READS the pool and prints what's in it:
  //   - every map end (scripts shutting down): one line, how full it is
  //   - omen_memstat (console): the full report
  //   - right before that error: the full report, so the log shows what
  //     filled it
  // Also written to omen_memstat.txt (next to the game).
  //
  // Pool layout (iw4x.exe, verified in the code that uses it):
  //   0x1DC0100  2048 dwords, one bit per slot, bit SET = slot free,
  //              slot 0 = top bit (MT_AllocIndex uses bsr)
  //   0x1DC2200  pointer to the slots (SL_ConvertToString: buf + id*12 + 4)
  //   0x1DC2280  string hash table, 0x7000 entries of 8 bytes
  //              (SL_ShutdownSystem walks 1..0x6FFF): status bits 0x30000,
  //              string id in the second dword
  //   a string: 4 byte header (refCount u16, user u8, len u8), then text
  // =====================================================================
  namespace
  {
    constexpr std::uintptr_t MT_FREE_BITS = 0x1DC0100;
    constexpr std::uintptr_t MT_BUFFER_PTR = 0x1DC2200;
    constexpr std::uintptr_t SL_HASH_TABLE = 0x1DC2280;
    constexpr int SL_HASH_SIZE = 0x7000;
    constexpr int MT_NODES = 65536;
    constexpr int MT_NODE_SIZE = 12;

    int lastUsedNodes = -1;

    struct StringInfo
    {
      std::string text;
      int nodes;
      int refCount;
      int user;
    };

    struct PoolStats
    {
      int freeNodes = 0;
      int largestFree = 0;
      int strings = 0;
      int stringNodes = 0;
      int userNodes[8]{};
      int userStrings[8]{};
      std::vector<StringInfo> list;
    };

    bool NodeFree(const std::uint32_t* bits, const int node)
    {
      return ((bits[node >> 5] >> (31 - (node & 31))) & 1) != 0;
    }

    PoolStats Gather(const bool withList)
    {
      PoolStats s;

      const auto* bits = reinterpret_cast<const std::uint32_t*>(MT_FREE_BITS);
      auto run = 0;
      for (auto i = 0; i < MT_NODES; ++i)
      {
        if (NodeFree(bits, i))
        {
          ++s.freeNodes;
          if (++run > s.largestFree) s.largestFree = run;
        }
        else
        {
          run = 0;
        }
      }

      const auto* buffer = *reinterpret_cast<const std::uint8_t* const*>(MT_BUFFER_PTR);
      if (!buffer) return s;

      for (auto i = 1; i < SL_HASH_SIZE; ++i)
      {
        const auto* entry = reinterpret_cast<const std::uint32_t*>(SL_HASH_TABLE + i * 8);
        if ((entry[0] & 0x30000) == 0) continue;

        const auto id = entry[1];
        if (id == 0 || id >= static_cast<std::uint32_t>(MT_NODES)) continue;

        const auto* ref = buffer + id * MT_NODE_SIZE;
        const auto refCount = *reinterpret_cast<const std::uint16_t*>(ref);
        const auto user = ref[2];
        const auto* text = reinterpret_cast<const char*>(ref + 4);
        const auto maxLen = static_cast<std::size_t>((MT_NODES - id) * MT_NODE_SIZE - 5);
        const auto len = strnlen(text, maxLen);
        const auto nodes = static_cast<int>((4 + len + 1 + MT_NODE_SIZE - 1) / MT_NODE_SIZE);

        ++s.strings;
        s.stringNodes += nodes;
        for (auto b = 0; b < 8; ++b)
        {
          if (user & (1 << b))
          {
            s.userNodes[b] += nodes;
            ++s.userStrings[b];
          }
        }

        if (withList)
        {
          s.list.push_back({ std::string(text, len), nodes, refCount, user });
        }
      }

      return s;
    }

    // "omen_modpreset_123_4" -> "omen_modpreset_#_#" so look-alike strings group
    std::string Shape(const std::string& text)
    {
      std::string out;
      auto lastDigit = false;
      for (const auto c : text)
      {
        if (out.size() >= 48) { out += "..."; break; }
        const auto digit = c >= '0' && c <= '9';
        if (digit)
        {
          if (!lastDigit) out += '#';
        }
        else if (c == '\n' || c == '\r')
        {
          out += ' ';
        }
        else
        {
          out += c;
        }
        lastDigit = digit;
      }
      return out;
    }

    std::string Printable(const std::string& text, const std::size_t max)
    {
      std::string out;
      for (const auto c : text)
      {
        if (out.size() >= max) { out += "..."; break; }
        out += (c == '\n' || c == '\r') ? ' ' : c;
      }
      return out;
    }
  }

  void MemStat::Report(const char* when, const bool full)
  {
    const auto s = Gather(full);
    const auto used = MT_NODES - s.freeNodes;
    const auto other = std::max(0, used - s.stringNodes);
    const auto change = lastUsedNodes < 0 ? 0 : used - lastUsedNodes;
    lastUsedNodes = used;

    std::string out = std::format("[memstat] {}: script memory {}% used ({} of {} slots, {} KB), biggest free piece {} bytes, "
      "strings {} ({} KB), other {} KB, change since last report {:+} slots\n",
      when, used * 100 / MT_NODES, used, MT_NODES, used * MT_NODE_SIZE / 1024, s.largestFree * MT_NODE_SIZE,
      s.strings, s.stringNodes * MT_NODE_SIZE / 1024, other * MT_NODE_SIZE / 1024, change);

    if (full)
    {
      out += "[memstat] strings by owner (bit: count / KB):";
      for (auto b = 0; b < 8; ++b)
      {
        if (s.userStrings[b]) out += std::format("  {}: {} / {}", b, s.userStrings[b], s.userNodes[b] * MT_NODE_SIZE / 1024);
      }
      out += "\n";

      // groups of look-alike strings, biggest first
      std::unordered_map<std::string, std::pair<int, int>> groups; // shape -> count, nodes
      for (const auto& str : s.list)
      {
        auto& g = groups[Shape(str.text)];
        ++g.first;
        g.second += str.nodes;
      }
      std::vector<std::pair<std::string, std::pair<int, int>>> sorted(groups.begin(), groups.end());
      std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });

      out += "[memstat] top 30 kinds of strings (KB, how many, what they look like):\n";
      for (std::size_t i = 0; i < sorted.size() && i < 30; ++i)
      {
        out += std::format("[memstat]   {:>5} KB  x{:<6} {}\n", sorted[i].second.second * MT_NODE_SIZE / 1024, sorted[i].second.first, sorted[i].first);
      }

      // biggest single strings
      auto list = s.list;
      std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.nodes > b.nodes; });
      out += "[memstat] 15 biggest strings (bytes, refs, owner bits, start):\n";
      for (std::size_t i = 0; i < list.size() && i < 15; ++i)
      {
        out += std::format("[memstat]   {:>6} b  refs {:<5} owner {:#04x}  {}\n", list[i].nodes * MT_NODE_SIZE, list[i].refCount, list[i].user, Printable(list[i].text, 80));
      }
      out += "[memstat] (\"other\" = not strings: mostly saved stacks of waiting script threads)\n";
    }

    Logger::Print("{}", out);

    try
    {
      Utils::IO::WriteFile("omen_memstat.txt", out, true);
    }
    catch (...)
    {
    }
  }

  // MT_AllocIndex's Com_Error(ERR_DROP, "%s: failed allocation of %d bytes for script usage", ...)
  void MemStat::MT_AllocFail_Hk(const int code, const char* fmt, const char* who, const int size)
  {
    Report(std::format("FAILED to get {} bytes", size).data(), true);
    Utils::Hook::Call<void(int, const char*, const char*, int)>(0x4B22D0)(code, fmt, who, size);
  }

  MemStat::MemStat()
  {
    Utils::Hook(0x4BF05D, MT_AllocFail_Hk, HOOK_CALL).install()->quick();

    Command::Add("omen_memstat", []
    {
      Report("now", true);
    });

    // (no report at "SV init": that runs once while the game is still
    // starting up - the map name doesn't exist yet and reading it stopped
    // the game from launching)
    Events::OnVMShutdown([]
    {
      const auto* mapname = Game::sv_mapname ? *Game::sv_mapname : nullptr;
      const auto* name = (mapname && mapname->current.string) ? mapname->current.string : "?";
      Report(std::format("map end ({})", name).data(), false);
    });
  }
}

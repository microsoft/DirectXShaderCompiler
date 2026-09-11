///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// WEXAdapterLog.cpp                                                         //
//                                                                           //
// Implements the WEX::Logging::Log functions declared in WEXAdapter.h for   //
// non-Windows builds.                                                       //
//                                                                           //
// Logging model:                                                            //
//   * Comment() and Error() append to a per-test buffer.                    //
//   * StartGroup()/EndGroup() bracket a sub-test scope; messages and        //
//     failures inside a group are scoped to that group.  When EndGroup()    //
//     fires, the group's content is appended to the outer buffer only if    //
//     at least one failure was recorded inside it; otherwise it is          //
//     discarded.  This matches the TAEF/WEX behavior on Windows.            //
//   * Error() records a non-fatal GoogleTest failure so the test is         //
//     marked failed; the originating file/line of the ADD_FAILURE is        //
//     suppressed by the FailurePrinter listener in TestMain.cpp so it       //
//     doesn't add noise -- the group name and Error message are what is     //
//     informative.                                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32

#include "dxc/Test/WEXAdapter.h"

#include <clocale>
#include <cstdio>
#include <cwchar>
#include <string>

namespace {

struct LogState {
  std::string TestBuffer;
  std::string GroupBuffer;
  std::string GroupName;
  unsigned GroupFailureCount = 0;
};

LogState &State() {
  static LogState S;
  return S;
}

std::string WideToUtf8(const wchar_t *Msg) {
  if (!Msg)
    return {};
  std::mbstate_t MBState{};
  const wchar_t *Src = Msg;
  size_t Bytes = std::wcsrtombs(nullptr, &Src, 0, &MBState);
  if (Bytes != static_cast<size_t>(-1)) {
    std::string Result(Bytes, '\0');
    Src = Msg;
    MBState = {};
    std::wcsrtombs(&Result[0], &Src, Bytes, &MBState);
    return Result;
  }
  // Locale conversion unavailable -- copy ASCII verbatim, replace rest.
  std::string Result;
  for (const wchar_t *P = Msg; *P; ++P)
    Result.push_back(*P < 0x80 ? static_cast<char>(*P) : '?');
  return Result;
}

std::string &ActiveBuffer(LogState &S) {
  return S.GroupName.empty() ? S.TestBuffer : S.GroupBuffer;
}

} // namespace

namespace WEX {
namespace Logging {
namespace Log {

void StartGroup(const wchar_t *Name) {
  LogState &S = State();
  // If a previous group was left open (missing EndGroup), surface its
  // contents conservatively rather than dropping them.
  if (!S.GroupName.empty()) {
    if (S.GroupFailureCount > 0 || !S.GroupBuffer.empty())
      S.TestBuffer.append(S.GroupBuffer);
    S.GroupBuffer.clear();
    S.GroupFailureCount = 0;
  }
  S.GroupName = WideToUtf8(Name);
}

void EndGroup(const wchar_t *Name) {
  LogState &S = State();
  if (S.GroupName.empty())
    return;
  if (S.GroupFailureCount > 0) {
    S.TestBuffer.append("---- FAILED: ");
    S.TestBuffer.append(S.GroupName);
    S.TestBuffer.append(" ----\n");
    S.TestBuffer.append(S.GroupBuffer);
  }
  S.GroupBuffer.clear();
  S.GroupName.clear();
  S.GroupFailureCount = 0;
  (void)Name; // EndGroup's name is informational; group state is single-deep.
}

void Comment(const wchar_t *Msg) {
  LogState &S = State();
  ActiveBuffer(S).append(WideToUtf8(Msg));
  ActiveBuffer(S).push_back('\n');
}

void Error(const wchar_t *Msg) {
  LogState &S = State();
  std::string MsgUtf8 = WideToUtf8(Msg);
  ActiveBuffer(S).append("ERROR: ");
  ActiveBuffer(S).append(MsgUtf8);
  ActiveBuffer(S).push_back('\n');
  if (!S.GroupName.empty())
    ++S.GroupFailureCount;
  ADD_FAILURE();
}

const char *GetBufferedLog() { return State().TestBuffer.c_str(); }
bool HasBufferedLog() { return !State().TestBuffer.empty(); }
void ClearBufferedLog() {
  LogState &S = State();
  S.TestBuffer.clear();
  S.GroupBuffer.clear();
  S.GroupName.clear();
  S.GroupFailureCount = 0;
}

void NotifyTestPartFailed() {
  LogState &S = State();
  if (!S.GroupName.empty())
    ++S.GroupFailureCount;
}

} // namespace Log
} // namespace Logging
} // namespace WEX

#endif // _WIN32

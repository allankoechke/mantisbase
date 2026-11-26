// MinGW's CRT runs C++ static destructors from DllMainCRTStartup on
// DLL_PROCESS_DETACH, and it calls the user's DllMain first. By the time
// Python reaches ExitProcess, Py_Finalize has already dropped the extension's
// objects. The destructors that still run belong to libstdc++, libwinpthread,
// and function-local singletons inside libmantisbase (drogon's HttpAppFramework,
// spdlog's registry). Those fault, so a process that printed "OK" still exits
// 139.
//
// TerminateProcess does not deliver further DLL_PROCESS_DETACH calls, so those
// destructors never run. ExitProcess has already published the interpreter's
// status; a failing run must keep that status.

#ifdef __MINGW32__

#define WIN32_LEAN_AND_MEAN
#include <cstdio>
#include <windows.h>

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH && reserved != nullptr) {
        fflush(stdout);
        fflush(stderr);
        DWORD code = STILL_ACTIVE;
        if (GetExitCodeProcess(GetCurrentProcess(), &code) && code != STILL_ACTIVE)
            TerminateProcess(GetCurrentProcess(), code);
    }
    return TRUE;
}

#endif

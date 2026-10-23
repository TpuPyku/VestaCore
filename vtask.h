// vtask.h - VestaCore: the task manager's data - running processes and top-level windows, the CPU and memory
// load (VestaHome and VestaLaunch draw it each in its own look).
//
// Processes come from toolhelp.dll (loaded at run time, without the heap lists), a process's CPU is the sum
// of its threads' GetThreadTimes over the last period, its memory the committed private pages (VirtualQueryEx),
// the whole CPU from GetIdleTime (PC: GetSystemTimes). Nothing runs in the background: the caller samples
// (every 1-2 s while its screen is open), the CPU figures are over the time since the previous sample.
// Plain Win32 (COREDLL on CE), no C++11.
#ifndef VTASK_H
#define VTASK_H

#include <windows.h>

#ifdef UNDER_CE
enum { TASK_PROC_MAX = 96, TASK_WIN_MAX = 48, TASK_HIST = 60 };
#else
enum { TASK_PROC_MAX = 400, TASK_WIN_MAX = 48, TASK_HIST = 60 };   // a desktop runs a few hundred
#endif
enum { TS_NAME, TS_CPU, TS_MEM, TS_PID };   // sort keys

struct TaskProc {
    DWORD   pid;
    wchar_t name[40];        // the exe's name
    int     threads;
    int     cpu10;           // its share of the CPU, tenths of a percent; -1 = not known yet
    DWORD   memKB;           // committed private memory; 0 = not known
    unsigned __int64 time;   // its threads' kernel + user time, 100 ns (for the next sample)
};

struct TaskWin {
    HWND    h;
    DWORD   pid;
    wchar_t title[64];
    wchar_t proc[40];        // the process's exe, "" if not in the list
};

struct TaskInfo {
    TaskProc proc[TASK_PROC_MAX];
    int      procCount;
    bool     procOk;          // the process list is available (toolhelp.dll)
    TaskWin  win[TASK_WIN_MAX];
    int      winCount;
    int      cpu10;           // the whole CPU, tenths of a percent; -1 = not known yet
    DWORD    memTotalKB, memAvailKB;
    unsigned char cpuHist[TASK_HIST], memHist[TASK_HIST];   // percent, oldest first
    int      histCount;
    int      sortKey;         // TS_*
    bool     sortDesc;
};

// A new sample into t (its sort order is kept). self: the caller's window, left out of the window list.
void TaskSample(TaskInfo* t, HWND self);
// Sorts t->proc by key (TS_*); the same key again turns the order round.
void TaskSort(TaskInfo* t, int key);
// Ends a process (exe: its name, see TaskIsSystem); false = refused (a system process, the caller itself)
// or failed.
bool TaskKill(DWORD pid, const wchar_t* exe);
// The unit's system processes (nk.exe, device.exe, gwes.exe ..., the firmware's Main.exe, G3NavHMI.exe, HzWD.exe,
// dr_Service.exe): ending them hangs or resets the unit.
bool TaskIsSystem(const wchar_t* exe);

#endif

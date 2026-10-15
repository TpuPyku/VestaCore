// vtask.cpp - VestaCore: processes, windows, CPU and memory load (see vtask.h).
#include "vtask.h"
#include <tlhelp32.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNDER_CE
#define TASK_PROC(lib, name) GetProcAddress(lib, L##name)
#ifndef TH32CS_SNAPNOHEAPS
#define TH32CS_SNAPNOHEAPS 0x40000000   // CE 5+: no heap lists (the 4.2 SDK header lacks it); they are slow and big
#endif
#define TASK_SNAP (TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD | TH32CS_SNAPNOHEAPS)
#define TASK_USER_TOP 0x40000000        // CE 6: a process's own space (exe, heaps, stacks); DLLs live above
#else
#define TASK_PROC(lib, name) GetProcAddress(lib, name)
#define TASK_SNAP (TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD)
#define TASK_USER_TOP 0x7FFF0000
#endif

typedef HANDLE (WINAPI *SnapFn)(DWORD, DWORD);
typedef BOOL   (WINAPI *ProcFn)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *ThreadFn)(HANDLE, THREADENTRY32*);
typedef BOOL   (WINAPI *CloseSnapFn)(HANDLE);
typedef HANDLE (WINAPI *OpenThreadFn)(DWORD, BOOL, DWORD);
typedef DWORD  (WINAPI *IdleFn)(void);
typedef BOOL   (WINAPI *SysTimesFn)(FILETIME*, FILETIME*, FILETIME*);
typedef DWORD  (WINAPI *QueryExFn)(HANDLE, LPCVOID, MEMORY_BASIC_INFORMATION*, DWORD);

static struct {
    int          state;   // 0 not loaded, 1 ok, -1 no toolhelp
    SnapFn       snap;
    ProcFn       procFirst, procNext;
    ThreadFn     threadFirst, threadNext;
    CloseSnapFn  closeSnap;
    OpenThreadFn openThread;
    IdleFn       idle;
    SysTimesFn   sysTimes;
    QueryExFn    queryEx;
} s_fn;

static void Load()
{
    if (s_fn.state)
        return;
#ifdef UNDER_CE
    HMODULE th = LoadLibrary(L"toolhelp.dll");
    HMODULE core = GetModuleHandle(L"coredll.dll");
    if (th) {
        s_fn.snap        = (SnapFn)TASK_PROC(th, "CreateToolhelp32Snapshot");
        s_fn.procFirst   = (ProcFn)TASK_PROC(th, "Process32First");
        s_fn.procNext    = (ProcFn)TASK_PROC(th, "Process32Next");
        s_fn.threadFirst = (ThreadFn)TASK_PROC(th, "Thread32First");
        s_fn.threadNext  = (ThreadFn)TASK_PROC(th, "Thread32Next");
        s_fn.closeSnap   = (CloseSnapFn)TASK_PROC(th, "CloseToolhelp32Snapshot");
    }
    if (core) {
        s_fn.openThread = (OpenThreadFn)TASK_PROC(core, "OpenThread");
        s_fn.idle       = (IdleFn)TASK_PROC(core, "GetIdleTime");
        s_fn.queryEx    = (QueryExFn)TASK_PROC(core, "VirtualQueryEx");
    }
#else
    HMODULE k = GetModuleHandle(L"kernel32.dll");
    s_fn.snap        = (SnapFn)TASK_PROC(k, "CreateToolhelp32Snapshot");
    s_fn.procFirst   = (ProcFn)TASK_PROC(k, "Process32FirstW");
    s_fn.procNext    = (ProcFn)TASK_PROC(k, "Process32NextW");
    s_fn.threadFirst = (ThreadFn)TASK_PROC(k, "Thread32First");
    s_fn.threadNext  = (ThreadFn)TASK_PROC(k, "Thread32Next");
    s_fn.openThread  = (OpenThreadFn)TASK_PROC(k, "OpenThread");
    s_fn.sysTimes    = (SysTimesFn)TASK_PROC(k, "GetSystemTimes");
    s_fn.queryEx     = (QueryExFn)TASK_PROC(k, "VirtualQueryEx");
#endif
    s_fn.state = s_fn.snap && s_fn.procFirst && s_fn.procNext ? 1 : -1;
}

static void CloseSnap(HANDLE h)
{
    if (s_fn.closeSnap)
        s_fn.closeSnap(h);
    else
        CloseHandle(h);
}

static unsigned __int64 Ft(const FILETIME& f)
{
    return ((unsigned __int64)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

static HANDLE OpenProc(DWORD pid)
{
#ifdef UNDER_CE
    return OpenProcess(0, FALSE, pid);
#else
    return OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
#endif
}

// Committed private memory of a process, KB; 0 = can't be read.
static DWORD ProcMemKB(DWORD pid)
{
    if (!s_fn.queryEx)
        return 0;
    HANDLE h = OpenProc(pid);
    if (!h)
        return 0;
    DWORD kb = 0, addr = 0x10000;
    for (int n = 0; n < 20000 && addr < TASK_USER_TOP; n++) {
        MEMORY_BASIC_INFORMATION mi;
        if (!s_fn.queryEx(h, (LPCVOID)addr, &mi, sizeof(mi)))
            break;
        if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE)
            kb += (DWORD)(mi.RegionSize / 1024);
        DWORD next = (DWORD)mi.BaseAddress + (DWORD)mi.RegionSize;
        if (next <= addr)
            break;
        addr = next;
    }
    CloseHandle(h);
    return kb;
}

// ---- sorting

static int s_key;
static bool s_desc;

static int CmpProc(const void* pa, const void* pb)
{
    const TaskProc* a = (const TaskProc*)pa;
    const TaskProc* b = (const TaskProc*)pb;
    int r;
    if (s_key == TS_CPU)
        r = a->cpu10 < b->cpu10 ? -1 : a->cpu10 > b->cpu10;
    else if (s_key == TS_MEM)
        r = a->memKB < b->memKB ? -1 : a->memKB > b->memKB;
    else if (s_key == TS_PID)
        r = a->pid < b->pid ? -1 : a->pid > b->pid;
    else
        r = _wcsicmp(a->name, b->name);
    if (s_desc)
        r = -r;
    return r ? r : _wcsicmp(a->name, b->name);   // equal ones by name, always from the start
}

static void SortNow(TaskInfo* t)
{
    s_key = t->sortKey;
    s_desc = t->sortDesc;
    qsort(t->proc, t->procCount, sizeof(TaskProc), CmpProc);
}

void TaskSort(TaskInfo* t, int key)
{
    if (key == t->sortKey) {
        t->sortDesc = !t->sortDesc;
    } else {
        t->sortKey = key;
        t->sortDesc = key == TS_CPU || key == TS_MEM;   // the busiest first, names and ids from the start
    }
    SortNow(t);
}

// ---- windows

struct WinCtx { TaskInfo* t; HWND self; };

static BOOL CALLBACK EnumWin(HWND h, LPARAM lp)
{
    WinCtx* c = (WinCtx*)lp;
    TaskInfo* t = c->t;
    if (t->winCount >= TASK_WIN_MAX)
        return FALSE;
    if (h == c->self || !IsWindowVisible(h) || GetParent(h))
        return TRUE;
    wchar_t cls[40] = L"";
    GetClassName(h, cls, 40);
    if (!wcscmp(cls, L"DesktopExplorerWindow") || !wcscmp(cls, L"HHTaskBar") || !wcscmp(cls, L"Progman") ||
        !wcscmp(cls, L"Shell_TrayWnd"))
        return TRUE;   // the desktop and the taskbar
    TaskWin& w = t->win[t->winCount];
    w.title[0] = 0;
    GetWindowText(h, w.title, 64);
    w.title[63] = 0;
    if (!w.title[0])
        return TRUE;
    w.h = h;
    w.pid = 0;
    GetWindowThreadProcessId(h, &w.pid);
    w.proc[0] = 0;
    t->winCount++;
    return TRUE;
}

// ---- the sample

void TaskSample(TaskInfo* t, HWND self)
{
    static TaskProc s_old[TASK_PROC_MAX];
    static int      s_oldCount;
    static DWORD    s_tick, s_idle;
    static unsigned __int64 s_sysIdle, s_sysAll;
    static int      s_round;
    Load();
    DWORD now = GetTickCount();
    unsigned __int64 wall = s_tick ? (unsigned __int64)(now - s_tick) * 10000 : 0;   // 100 ns
    bool memRound = s_round++ % 3 == 0;   // the memory walk is the slow part: every third sample

    memcpy(s_old, t->proc, t->procCount * sizeof(TaskProc));
    s_oldCount = t->procCount;
    t->procCount = 0;
    t->procOk = s_fn.state > 0;
    if (t->procOk) {
        HANDLE snap = s_fn.snap(TASK_SNAP, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe;
            pe.dwSize = sizeof(pe);
            for (BOOL ok = s_fn.procFirst(snap, &pe); ok && t->procCount < TASK_PROC_MAX; ok = s_fn.procNext(snap, &pe)) {
                TaskProc& p = t->proc[t->procCount++];
                memset(&p, 0, sizeof(p));
                p.pid = pe.th32ProcessID;
                const wchar_t* n = wcsrchr(pe.szExeFile, L'\\');
                wcsncpy(p.name, n ? n + 1 : pe.szExeFile, 39);
                p.name[39] = 0;
                p.threads = (int)pe.cntThreads;
                pe.dwSize = sizeof(pe);
            }
            if (s_fn.threadFirst && s_fn.threadNext) {   // threads' times, summed by process
                THREADENTRY32 te;
                te.dwSize = sizeof(te);
                for (BOOL ok = s_fn.threadFirst(snap, &te); ok; ok = s_fn.threadNext(snap, &te)) {
                    TaskProc* p = NULL;
                    for (int i = 0; i < t->procCount && !p; i++)
                        if (t->proc[i].pid == te.th32OwnerProcessID)
                            p = &t->proc[i];
                    te.dwSize = sizeof(te);
                    if (!p)
                        continue;
                    HANDLE th = s_fn.openThread ? s_fn.openThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID) : NULL;
                    bool own = th != NULL;
#ifdef UNDER_CE
                    if (!th)
                        th = (HANDLE)te.th32ThreadID;   // before CE 6 a thread's id is its handle
#endif
                    FILETIME c, e, k, u;
                    if (th && GetThreadTimes(th, &c, &e, &k, &u))
                        p->time += Ft(k) + Ft(u);
                    if (own)
                        CloseHandle(th);
                }
            }
            CloseSnap(snap);
        }
    }
    int sum10 = 0;
    for (int i = 0; i < t->procCount; i++) {
        TaskProc& p = t->proc[i];
        const TaskProc* o = NULL;
        for (int j = 0; j < s_oldCount && !o; j++)
            if (s_old[j].pid == p.pid)
                o = &s_old[j];
        p.cpu10 = -1;
        if (o && wall) {
            unsigned __int64 d = p.time > o->time ? p.time - o->time : 0;   // a thread ended: its time is gone
            p.cpu10 = (int)(d * 1000 / wall);
            if (p.cpu10 > 1000)
                p.cpu10 = 1000;
            sum10 += p.cpu10;
        }
        p.memKB = memRound || !o ? ProcMemKB(p.pid) : o->memKB;
    }

    // the whole CPU
    t->cpu10 = -1;
#ifdef UNDER_CE
    DWORD idle = s_fn.idle ? s_fn.idle() : 0xFFFFFFFF;
    if (idle != 0xFFFFFFFF) {
        if (s_tick && now != s_tick) {
            DWORD di = idle - s_idle, dt = now - s_tick;
            t->cpu10 = di >= dt ? 0 : (int)((dt - di) * 1000 / dt);
        }
        s_idle = idle;
    } else if (wall) {
        t->cpu10 = sum10 > 1000 ? 1000 : sum10;   // no idle counter: the processes' sum
    }
#else
    FILETIME fi, fk, fu;
    if (s_fn.sysTimes && s_fn.sysTimes(&fi, &fk, &fu)) {   // kernel time includes the idle time
        unsigned __int64 idle = Ft(fi), all = Ft(fk) + Ft(fu);
        if (s_sysAll && all > s_sysAll)
            t->cpu10 = (int)((all - s_sysAll - (idle - s_sysIdle)) * 1000 / (all - s_sysAll));
        s_sysIdle = idle;
        s_sysAll = all;
    } else if (wall) {
        t->cpu10 = sum10 > 1000 ? 1000 : sum10;
    }
#endif
    s_tick = now;

    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    t->memTotalKB = (DWORD)(ms.dwTotalPhys / 1024);
    t->memAvailKB = (DWORD)(ms.dwAvailPhys / 1024);

    if (t->cpu10 >= 0) {   // the graphs: from the second sample on
        if (t->histCount == TASK_HIST) {
            memmove(t->cpuHist, t->cpuHist + 1, TASK_HIST - 1);
            memmove(t->memHist, t->memHist + 1, TASK_HIST - 1);
            t->histCount--;
        }
        t->cpuHist[t->histCount] = (unsigned char)((t->cpu10 + 5) / 10);
        t->memHist[t->histCount] = (unsigned char)(t->memTotalKB ?
                                   (t->memTotalKB - t->memAvailKB) * 100 / t->memTotalKB : 0);
        t->histCount++;
    }

    t->winCount = 0;
    WinCtx c = { t, self };
    EnumWindows(EnumWin, (LPARAM)&c);
    for (int i = 0; i < t->winCount; i++)
        for (int j = 0; j < t->procCount; j++)
            if (t->proc[j].pid == t->win[i].pid)
                wcscpy(t->win[i].proc, t->proc[j].name);

    SortNow(t);
}

// ---- ending a process

bool TaskIsSystem(const wchar_t* exe)
{
    static const wchar_t* sys[] = { L"nk.exe", L"filesys.exe", L"device.exe", L"udevice.exe", L"gwes.exe",
                                    L"services.exe", L"servicesd.exe", L"srvtrust.exe",
                                    L"System", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe",
                                    L"lsass.exe" };
    for (int i = 0; i < (int)(sizeof(sys) / sizeof(sys[0])); i++)
        if (!_wcsicmp(exe, sys[i]))
            return true;
    return false;
}

bool TaskKill(DWORD pid, const wchar_t* exe)
{
    if (pid == GetCurrentProcessId() || TaskIsSystem(exe))
        return false;
#ifdef UNDER_CE
    HANDLE h = OpenProcess(0, FALSE, pid);
#else
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
#endif
    if (!h)
        return false;
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok != FALSE;
}

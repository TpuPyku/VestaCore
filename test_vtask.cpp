// test_vtask.cpp - the task manager's load (vtask.cpp) on the PC (test_vtask.bat): two samples kept apart.
#include <windows.h>
#include <stdio.h>
#include "vtask.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static void Burn(DWORD ms)   // this thread busy that long
{
    DWORD t0 = GetTickCount();
    volatile unsigned x = 0;
    while (GetTickCount() - t0 < ms)
        x++;
}

static int Own(const TaskInfo& t)   // this process's share, tenths of a percent; -1 none
{
    for (int i = 0; i < t.procCount; i++)
        if (t.proc[i].pid == GetCurrentProcessId())
            return t.proc[i].cpu10;
    return -1;
}

static TaskInfo s_slow, s_fast;   // (big: not on the stack)

int main()
{
    // HU 2026-11-05: every process at 100 % in the load log - its sample once a minute, the mix list's every 2 s;
    // the time since the last sample was shared: a minute's CPU over 2 s. Here: 2 s with 0.5 s busy, the other
    // sample 0.1 s before the end - this one's share stays ~25 %.
    TaskSample(&s_slow, NULL);
    TaskSample(&s_fast, NULL);
    Burn(500);
    Sleep(1400);
    TaskSample(&s_fast, NULL);
    Sleep(100);
    TaskSample(&s_slow, NULL);
    int slow = Own(s_slow);
    printf("own share: %d.%d %%\n", slow / 10, slow % 10);
    Check(s_slow.procOk, "the process list read");
    Check(slow >= 150 && slow <= 400, "a sample's share over its own time since its last (~25 %, not 100 %)");
    printf("%d of %d checks failed\n", s_failed, s_checks);
    return s_failed;
}

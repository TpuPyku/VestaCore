// test_vrvc.cpp - which video input the camera is looked for on (vrvc.h RvcInputs), on the PC.
#include <windows.h>
#include <stdio.h>
#include "vrvc.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

int main()
{
    int in[2];
    Check(RvcInputs(-1, in) == 2 && in[0] == 0 && in[1] == 1, "nothing found yet: the rear camera's input 0, then 1");
    Check(RvcInputs(0, in) == 2 && in[0] == 0 && in[1] == 1, "found on 0: 0 first");
    Check(RvcInputs(1, in) == 2 && in[0] == 1 && in[1] == 0,
          "found on 1 (the shell's camera type 3 - a 4125 Vesta 2021, 2026-11-23): 1 first, then 0");
    Check(RvcInputs(7, in) == 2 && in[0] == 0 && in[1] == 1, "a stray number: as nothing found");
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

// test_mcuring.h - tests: an MCU ring of their own (vmcushm.h), written as the proxy writes it.
#ifndef TEST_MCURING_H
#define TEST_MCURING_H

#include <string.h>
#include "vmcushm.h"

static McuShm* TestRing(const wchar_t* name)   // a fresh ring of this name (the handle kept: it lives with the test)
{
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(McuShm), name);
    McuShm* r = (McuShm*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(McuShm));
    memset(r, 0, sizeof(*r));
    r->magic = MCU_SHM_MAGIC;
    return r;
}

static void TestPost(McuShm* r, unsigned short id, const unsigned char* d, int len)
{
    LONG n = InterlockedIncrement((LONG*)&r->seq);
    McuMsg* m = &r->msg[(n - 1) & (MCU_SHM_SLOTS - 1)];
    m->seq = 0;
    m->id = id;
    m->len = (unsigned char)len;
    memset(m->data, 0, MCU_SHM_DATA);
    memcpy(m->data, d, len < MCU_SHM_DATA ? len : MCU_SHM_DATA);
    m->seq = n;
}

// The machine ID 41800514 180624 <num> as the MCU answers the product info.
static void TestMachinePost(McuShm* r, const char* num)
{
    static const unsigned char a[5] = { 0, 0x41, 0x80, 0x05, 0x14 }, b[5] = { 0, 0x20, 0x18, 0x06, 0x24 };
    unsigned char c[16] = { 0 };
    int n = (int)strlen(num);
    memcpy(c + 1, num, n);
    TestPost(r, 0x4118, a, 5);
    TestPost(r, 0x4111, b, 5);
    TestPost(r, 0x4112, c, n + 1);
}

#endif

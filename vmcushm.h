// vmcushm.h - VestaCore: the layout of the MCU proxy's shared ring "VestaMCU2" (VestaHome/SPIPower/native/mcushm.h
// writes it - keep both identical): every message the head unit's MCU sends, 256 slots. Writer: n =
// InterlockedIncrement(&seq), slot (n - 1) % 256, the slot's seq set to n last. Readers: VestaHome (vh_mcu.inc) and
// vlicense (the machine ID: the unit's ID).
#ifndef VESTA_MCUSHM_H
#define VESTA_MCUSHM_H

#include <windows.h>

#define MCU_SHM_NAME   L"VestaMCU2"   // v2: 40 data bytes
#define MCU_SHM_MAGIC  0x55434D56     // "VMCU"
#define MCU_SHM_SLOTS  256
#define MCU_SHM_DATA   40

struct McuMsg {
    volatile LONG  seq;    // = the writer's n when the slot is complete
    DWORD          tick;   // GetTickCount() when it came
    unsigned short id;     // the MCU's message id
    unsigned char  len, skel;
    unsigned char  data[MCU_SHM_DATA];
};
struct McuShm {
    DWORD         magic, version;
    volatile LONG seq, attach, wraps;
    DWORD         reserved[3];
    McuMsg        msg[MCU_SHM_SLOTS];
};

#endif

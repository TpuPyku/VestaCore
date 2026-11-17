// The rear view camera as the stock shell shows it (CRvcDlg + CSkinRearViewCtrl of G3NavHMI, Ghidra 2026-10-17),
// for VestaHome without the shell (VestaLaunch: the reverse only; moved from VestaHome 2026-11-17): the reverse
// gear from desay_sdk_api.dll (GPIO 0x5d), the camera = video
// input channel 0 shown by the display driver where the screen has the colour key, the guide lines from the
// maker's RVS_algBackingTrace.dll (its calibration \ResidentFlash\navigation\app\Config\PARAMS.TXT): the
// static ones and, with the steering angle of the CAN module, the trajectory. All loaded at run time; the PC
// build has none of them (RvcReverse = -1, no lines).
#ifndef VESTA_VRVC_H
#define VESTA_VRVC_H

typedef void (*RvcLogFn)(const wchar_t* fmt, ...);

bool RvcInit(RvcLogFn log);    // once; false: no desay_sdk_api.dll
int  RvcReverse();             // 1 in reverse, 0 not, -1 unknown
int  RvcVideo(bool on);        // the camera's video under the colour key (on) / off; on: the signal (0 none)
COLORREF RvcKey();             // the colour key: where the screen has it, the camera shows
// The guide lines into dc (800 x 480): angle10 = the steering wheel's angle in tenths of a degree, valid = it is
// fresh (otherwise only the static lines, blue as the shell's without the angle).
void RvcDrawLines(HDC dc, int angle10, bool valid);

#endif

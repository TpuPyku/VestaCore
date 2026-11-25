// vlicense.h - VestaHome's support license (2026-11-15, the user: Android Auto's support splash goes away with a key
// for the head unit's ID). Nothing is locked without it. The author signs the unit's ID (tools/make_license.py sign,
// ECDSA P-256 over SHA-256 of "VestaHome support <level>|<ID>"); the user drops the file VestaLicense.key into
// VestaHome's folder. Checked offline: the public key is in vlicense_key.inc, the private one never in a repo.
// Levels (2026-11-18, the user): a subscriber - Android Auto's splash gone; a co-author (Boosty «Соавторы») - also
// the smart home add-on, paid in full. A file with no "level=" line (all before levels) is a subscriber's.
#ifndef VLICENSE_H
#define VLICENSE_H

#include <wchar.h>

enum { LICENSE_ID_LEN = 15, LICENSE_UNIT_MAX = 256, LICENSE_MACHINE_MAX = 48 };   // "VH-XXXXX-XXXXX" and its 0; the unit's bytes' room
enum { LICENSE_SRC_NONE, LICENSE_SRC_HAL, LICENSE_SRC_MAC, LICENSE_SRC_PC, LICENSE_SRC_MCU };
enum { LICENSE_NONE, LICENSE_SUPPORTER, LICENSE_COAUTHOR };   // what a license file gives: its level, 0 - nothing

// The unit's own bytes (VestaHome's and its add-ons' - the same ID in each, 2026-11-15): its machine ID from the MCU
// (2026-11-25: the HAL's device id is the same on every unit - VH-A6R3Y-CJN2Y everywhere), read by each program from the
// MCU proxy's shared ring (LicenseMachineRead: not the registry, which any registry editor changes - the user
// 2026-11-25); none yet - 0 bytes, no ID. The PC build with no machine ID: the system volume's serial number.
// Their count; source (may be NULL): LICENSE_SRC_*.
int LicenseUnitBytes(unsigned char* buf, int cap, int* source);
// The old bytes (until 2026-11-25): the HAL's device id, else the adapters' MAC addresses; the PC - as above. Only the
// passwords' old key (vsecret) uses them.
int LicenseLegacyBytes(unsigned char* buf, int cap);
// The machine ID's text (VestaHome: "<0x4118> <0x4111> <0x4112>" as "О системе" shows it) as the unit's bytes; 0 - not
// one (empty, not printable ASCII, longer than LICENSE_MACHINE_MAX).
int LicenseMachineBytes(const wchar_t* text, unsigned char* buf, int cap);
// The machine ID's parts from the MCU's messages (0x4118 00 <4 bytes>, 0x4111 00 20 <3 bytes>, 0x4112 00 <text>, as the
// stock "Info" asks them: SPApi_SendRadioRequestProductInfo); other messages - nothing.
struct LicenseMachine {
    wchar_t id[16], date[8], num[12];
};
void LicenseMachineTake(LicenseMachine* m, int msgId, const unsigned char* p, int len);
// Its text as "Details" shows it ("—" none yet; a part may be missing): true when all of it is there.
bool LicenseMachineText(const LicenseMachine* m, wchar_t* out, int n);
// The machine ID from the MCU proxy's ring (vmcushm.h), kept by the program once whole (it never changes; what is
// written into the ring after that changes nothing): false - not there (no proxy, or nobody asked the MCU lately).
bool LicenseMachineRead(wchar_t* text, int cap);
// Tests: the ring's name instead of VestaMCU2 (NULL - back), and the kept machine ID forgotten.
void LicenseMachineRing(const wchar_t* name);
// The unit's ID shown to the user: SHA-256 of its bytes, 50 bits in 10 letters.
void LicenseIdText(const unsigned char* hw, int n, wchar_t* out);
// This unit's ID ("" - its machine ID not known yet: no license fits).
void LicenseUnitId(wchar_t* out);
// A license file's text (len bytes) for this ID, against the public key `pub` (65 bytes, uncompressed P-256): its
// level (LICENSE_*; LICENSE_NONE - not valid, true-or-false use is "any level").
int LicenseVerify(const char* text, int len, const wchar_t* id, const unsigned char* pub, int publen);
// A signature (base64 of r||s) of msg by the key `pub` (NULL - the author's, vlicense_key.inc): ECDSA P-256 over SHA-256.
bool LicenseSigned(const char* sigB64, int sigLen, const char* msg, int m, const unsigned char* pub, int publen);
// The value of the line "<key>" (e.g. "sig=") in text: its start and length; false - none.
bool LicenseField(const char* text, int len, const char* key, const char** v, int* vlen);
// The same against the author's key (vlicense_key.inc).
int LicenseValid(const char* text, int len, const wchar_t* id);
// A license file at `path` (found: whether there is one; may be NULL) - against `pub`, or the author's key.
int LicenseFileVerify(const wchar_t* path, const wchar_t* id, const unsigned char* pub, int publen, bool* found);
int LicenseFileValid(const wchar_t* path, const wchar_t* id, bool* found);
// This unit's level, its ID into `id`: VestaLicense.key in VestaHome's folder (\ResidentFlash\VestaHome) or beside
// the running program - the higher of the two (VestaHome's add-ons check it themselves: VestaHome is public).
int LicenseUnitLevel(wchar_t* id);
// The key's path beside a program (MAX_PATH of room): its folder + "VestaLicense.key".
void LicenseKeyBeside(const wchar_t* program, wchar_t* path);

#endif

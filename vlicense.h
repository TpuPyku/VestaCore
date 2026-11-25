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
// (2026-11-25: the HAL's device id is the same on every unit - VH-A6R3Y-CJN2Y everywhere), kept for the boot by
// LicenseMachineSet; none yet - 0 bytes, no ID. The PC build with no machine ID: the system volume's serial number.
// Their count; source (may be NULL): LICENSE_SRC_*.
int LicenseUnitBytes(unsigned char* buf, int cap, int* source);
// The old bytes (until 2026-11-25): the HAL's device id, else the adapters' MAC addresses; the PC - as above. Only the
// passwords' old key (vsecret) uses them.
int LicenseLegacyBytes(unsigned char* buf, int cap);
// The machine ID's text (VestaHome: "<0x4118> <0x4111> <0x4112>" as "О системе" shows it) as the unit's bytes; 0 - not
// one (empty, not printable ASCII, longer than LICENSE_MACHINE_MAX).
int LicenseMachineBytes(const wchar_t* text, unsigned char* buf, int cap);
// The machine ID kept for this boot (the unit: HKLM\Software\Vesta "Machine" - the registry starts afresh at each
// boot; the PC: HKCU): VestaHome puts it when the MCU has told it, the add-ons read it. NULL - forgotten. False - not one.
bool LicenseMachineSet(const wchar_t* text);
bool LicenseMachineGet(wchar_t* text, int cap);   // false: none kept ("")
// The unit's ID shown to the user: SHA-256 of its bytes, 50 bits in 10 letters.
void LicenseIdText(const unsigned char* hw, int n, wchar_t* out);
// This unit's ID ("" - its machine ID not known yet: no license fits).
void LicenseUnitId(wchar_t* out);
// A license file's text (len bytes) for this ID, against the public key `pub` (65 bytes, uncompressed P-256): its
// level (LICENSE_*; LICENSE_NONE - not valid, true-or-false use is "any level").
int LicenseVerify(const char* text, int len, const wchar_t* id, const unsigned char* pub, int publen);
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

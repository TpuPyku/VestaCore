// vsecret.h - VestaCore: secrets kept in settings files (VestaHome's Wi-Fi passwords, VestaSmartHome's broker
// password): sealed with a key of this head unit, so that the file read elsewhere (another unit, a PC) does not
// give them away. The key = SHA-256 of the unit's bytes (vlicense.h LicenseUnitBytes: the MCU's machine ID; the PC: its
// volume) - one key for every program on the unit; ChaCha20 + HMAC-SHA-256 (BearSSL). The
// text form is "v1:" + hex (nonce, cipher text, tag); up to 127 bytes of UTF-8.
#ifndef VESTA_SECRET_H
#define VESTA_SECRET_H

// Before the settings are read, and again when the machine ID comes (LicenseMachineSet); the key's source:
// LICENSE_SRC_MCU, LICENSE_SRC_PC, 0 none yet (then the old key - the HAL's device id - seals, see below).
int  SecretInit();
bool SecretSeal(const wchar_t* plain, wchar_t* out, int cap);     // false: too long
bool SecretOpen(const wchar_t* sealed, wchar_t* plain, int cap);  // false: not sealed here, or damaged
bool SecretIsSealed(const wchar_t* s);
// Sealed with the old key (until 2026-11-25 the HAL's device id, the same on every unit) while the new one is there:
// open it and seal it again.
bool SecretNeedsReseal(const wchar_t* sealed);
// Bytes sealed the same way (the phone book's cache, pbcache.h): out = nonce, cipher text, tag. Their length, -1: out
// too small or no key. Open: the plain length in out, -1 - another unit's, damaged, or out too small.
int  SecretSealBytes(const unsigned char* plain, int n, unsigned char* out, int cap);
int  SecretOpenBytes(const unsigned char* sealed, int len, unsigned char* out, int cap);

#endif

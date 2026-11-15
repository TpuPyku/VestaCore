// vlicense.h - VestaHome's support license (2026-11-15, the user: Android Auto's support splash goes away with a key
// for the head unit's ID). Nothing is locked without it. The author signs the unit's ID (tools/make_license.py sign,
// ECDSA P-256 over SHA-256 of "VestaHome support 1|<ID>"); the user drops the file VestaLicense.key into VestaHome's
// folder. Checked offline: the public key is in vlicense_key.inc, the private one never in a repo.
#ifndef VLICENSE_H
#define VLICENSE_H

#include <wchar.h>

enum { LICENSE_ID_LEN = 15 };   // "VH-XXXXX-XXXXX" and its 0

// The unit's ID shown to the user: SHA-256 of its hardware bytes (the HAL's device id), 50 bits in 10 letters.
void LicenseIdText(const unsigned char* hw, int n, wchar_t* out);
// A license file's text (len bytes) for this ID, against the public key `pub` (65 bytes, uncompressed P-256).
bool LicenseVerify(const char* text, int len, const wchar_t* id, const unsigned char* pub, int publen);
// The same against the author's key (vlicense_key.inc).
bool LicenseValid(const char* text, int len, const wchar_t* id);

#endif

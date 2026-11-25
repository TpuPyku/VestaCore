// vupdate.h - VestaCore: VestaHome updated from its own "О системе" (the user 2026-11-25): the installer on the card
// tells its version (a mark in the exe, "VHINSTVER:<version>" and a 0 byte) and is signed by the author
// (VestaHomeInstall.sig beside it - tools/make_license.py sign-installer: ECDSA P-256 over SHA-256 of
// "VestaHome installer 1|<version>|<sha256 hex>"); VestaHome checks both, leaves a boot job (the path, the version, the
// exe's SHA-256), restarts; the loader (VestaStub) runs the installer before VestaHome and its add-ons start, only if
// the exe still has that SHA-256.
#ifndef VESTA_UPDATE_H
#define VESTA_UPDATE_H

#include <wchar.h>

enum { UPDATE_OK, UPDATE_NO_FILE, UPDATE_NO_MARK, UPDATE_NO_SIG, UPDATE_BAD_SIG };

// The version mark in n bytes: ver (cap with its 0); false - none, empty, unended or longer than the room ("").
bool UpdateMark(const unsigned char* b, int n, char* ver, int cap);
// Two versions by their numbers ("1.10.1" > "1.9.1"; a part more - later): <0, 0, >0.
int  UpdateVersionCmp(const char* a, const char* b);
// The exe's SHA-256 and version mark (read in chunks), then its signature file against pub (NULL - the author's key):
// UPDATE_*. ver - the exe's version whenever it has a mark.
int  UpdateCheck(const wchar_t* exe, const wchar_t* sig, const unsigned char* pub, int publen, char* ver, int cap,
                 unsigned char sha[32]);
// The exe's SHA-256 (and its version mark, ver may be NULL); false - not read.
bool UpdateFileScan(const wchar_t* exe, unsigned char sha[32], char* ver, int cap);
// The boot job's text: "path=", "version=", "sha256=" lines (UTF-8); its length, -1 no room.
int  UpdateJobText(const wchar_t* exe, const char* ver, const unsigned char sha[32], char* out, int cap);
bool UpdateJobParse(const char* t, int n, wchar_t* exe, int ecap, char* ver, int vcap, unsigned char sha[32]);

#endif

// vupdate.cpp - VestaCore: the installer's version, signature and boot job (see vupdate.h).
#include "vupdate.h"
#include "vlicense.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <bearssl.h>

static const char kMark[] = "VHINSTVER:";
enum { kMarkLen = sizeof(kMark) - 1, kChunk = 64 * 1024, kVerMax = 32 };

static bool MarkAt(const unsigned char* b, int n, int at, char* ver, int cap)   // the version after a mark at `at`
{
    int k = 0;
    while (at + k < n && b[at + k] && k < cap - 1)
        k++;
    if (!k || at + k >= n || b[at + k])
        return false;   // empty, unended, or no room
    memcpy(ver, b + at, k);
    ver[k] = 0;
    return true;
}

bool UpdateMark(const unsigned char* b, int n, char* ver, int cap)
{
    ver[0] = 0;
    for (int i = 0; i + kMarkLen < n; i++)   // (the first one with a version: VestaHome inside the installer has the
        if (b[i] == 'V' && !memcmp(b + i, kMark, kMarkLen) && MarkAt(b, n, i + kMarkLen, ver, cap))   // mark empty)
            return true;
    return false;
}

int UpdateVersionCmp(const char* a, const char* b)
{
    while (*a || *b) {
        if (!*a || !*b)
            return *a ? 1 : -1;   // (a part more: later)
        long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
        if (x != y)
            return x < y ? -1 : 1;
        a += *a == '.';
        b += *b == '.';
    }
    return 0;
}

bool UpdateFileScan(const wchar_t* exe, unsigned char sha[32], char* ver, int cap)
{
    HANDLE f = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    unsigned char* buf = (unsigned char*)malloc(kChunk + kMarkLen + kVerMax);
    br_sha256_context c;
    br_sha256_init(&c);
    int keep = 0;   // the last bytes of the chunk before: a mark across two chunks
    DWORD got = 0;
    if (ver)
        ver[0] = 0;
    while (buf && ReadFile(f, buf + keep, kChunk, &got, NULL) && got) {
        br_sha256_update(&c, buf + keep, got);
        if (ver && !ver[0])
            UpdateMark(buf, keep + (int)got, ver, cap);
        int all = keep + (int)got;
        keep = all < kMarkLen + kVerMax ? all : kMarkLen + kVerMax;
        memmove(buf, buf + all - keep, keep);
    }
    CloseHandle(f);
    br_sha256_out(&c, sha);
    bool ok = buf != NULL;
    free(buf);
    return ok;
}

static bool HexIs(const char* h, int n, const unsigned char* sha)   // 64 hex digits = sha
{
    if (n != 64)
        return false;
    for (int i = 0; i < 32; i++) {
        unsigned v;
        if (sscanf(h + i * 2, "%2x", &v) != 1 || v != sha[i])
            return false;
    }
    return true;
}

static int SigCheck(const wchar_t* sig, const char* ver, const unsigned char sha[32], const unsigned char* pub, int publen)
{
    char t[512];
    DWORD n = 0;
    HANDLE f = CreateFileW(sig, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return UPDATE_NO_SIG;
    ReadFile(f, t, sizeof(t) - 1, &n, NULL);
    CloseHandle(f);
    const char *fv, *fh, *fs;
    int vl, hl, sl;
    if (!LicenseField(t, n, "version=", &fv, &vl) || !LicenseField(t, n, "sha256=", &fh, &hl) ||
        !LicenseField(t, n, "sig=", &fs, &sl) || vl != (int)strlen(ver) || memcmp(fv, ver, vl) || !HexIs(fh, hl, sha))
        return UPDATE_BAD_SIG;   // another version or another exe
    char msg[160];
    int m = _snprintf(msg, sizeof(msg), "VestaHome installer 1|%s|%.64s", ver, fh);
    return LicenseSigned(fs, sl, msg, m, pub, publen) ? UPDATE_OK : UPDATE_BAD_SIG;
}

int UpdateCheck(const wchar_t* exe, const wchar_t* sig, const unsigned char* pub, int publen, char* ver, int cap,
                unsigned char sha[32])
{
    if (!UpdateFileScan(exe, sha, ver, cap))
        return UPDATE_NO_FILE;
    if (!ver[0])
        return UPDATE_NO_MARK;
    return SigCheck(sig, ver, sha, pub, publen);
}

int UpdateJobText(const wchar_t* exe, const char* ver, const unsigned char sha[32], char* out, int cap)
{
    char path[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, path, sizeof(path), NULL, NULL);
    int n = _snprintf(out, cap, "path=%s\nversion=%s\nsha256=", path, ver);
    if (n < 0 || n + 66 > cap)
        return -1;
    for (int i = 0; i < 32; i++)
        n += _snprintf(out + n, cap - n, "%02x", sha[i]);
    out[n++] = '\n';
    out[n] = 0;
    return n;
}

bool UpdateJobParse(const char* t, int n, wchar_t* exe, int ecap, char* ver, int vcap, unsigned char sha[32])
{
    const char *fp, *fv, *fh;
    int pl, vl, hl;
    if (!LicenseField(t, n, "path=", &fp, &pl) || !LicenseField(t, n, "version=", &fv, &vl) ||
        !LicenseField(t, n, "sha256=", &fh, &hl) || hl != 64 || vl >= vcap)
        return false;
    for (int i = 0; i < 32; i++) {
        unsigned v;
        if (sscanf(fh + i * 2, "%2x", &v) != 1)
            return false;
        sha[i] = (unsigned char)v;
    }
    memcpy(ver, fv, vl);
    ver[vl] = 0;
    int w = MultiByteToWideChar(CP_UTF8, 0, fp, pl, exe, ecap - 1);
    exe[w > 0 ? w : 0] = 0;
    return w > 0;
}

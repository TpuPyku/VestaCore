// vlicense.cpp - VestaHome's support license (vlicense.h): the unit's ID text, the license file checked by BearSSL.
#include "vlicense.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <bearssl.h>

#include "vlicense_key.inc"

static const char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // (no I, L, O, U: as make_license.py)

void LicenseIdText(const unsigned char* hw, int n, wchar_t* out)
{
    unsigned char d[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, hw, n);
    br_sha256_out(&c, d);
    unsigned __int64 v = 0;
    for (int i = 0; i < 7; i++)
        v = v << 8 | d[i];
    v >>= 6;   // 50 bits
    wcscpy(out, L"VH-");
    for (int i = 0, k = 3; i < 10; i++) {
        if (i == 5)
            out[k++] = L'-';
        out[k++] = (wchar_t)kAlphabet[(int)(v >> (5 * (9 - i))) & 31];
    }
    out[LICENSE_ID_LEN - 1] = 0;
}

// The value of the line "<key>=" in text (len bytes): its start and length; false - none
static bool Field(const char* text, int len, const char* key, const char** v, int* vlen)
{
    int kl = (int)strlen(key);
    for (int at = 0; at < len;) {
        int e = at;
        while (e < len && text[e] != '\n')
            e++;
        int end = e > at && text[e - 1] == '\r' ? e - 1 : e;
        if (end - at > kl && !memcmp(text + at, key, kl)) {
            *v = text + at + kl;
            *vlen = end - at - kl;
            return true;
        }
        at = e + 1;
    }
    return false;
}

static int B64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}

static int Base64(const char* s, int n, unsigned char* out, int cap)   // the bytes; -1 not base64 or too long
{
    int k = 0, bits = 0;
    unsigned acc = 0;
    for (int i = 0; i < n && s[i] != '='; i++) {
        int v = B64(s[i]);
        if (v < 0)
            return -1;
        acc = acc << 6 | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k == cap)
                return -1;
            out[k++] = (unsigned char)(acc >> bits);
        }
    }
    return k;
}

static int FileLevel(const char* text, int len)   // "level=1|2"; none - a subscriber; anything else - 0
{
    const char* v;
    int n;
    if (!Field(text, len, "level=", &v, &n))
        return LICENSE_SUPPORTER;
    return n == 1 && (*v == '1' || *v == '2') ? *v - '0' : LICENSE_NONE;
}

int LicenseVerify(const char* text, int len, const wchar_t* id, const unsigned char* pub, int publen)
{
    const char *fid, *fsig;
    int idLen, sigLen, level = FileLevel(text, len);
    if (!level || !Field(text, len, "id=", &fid, &idLen) || !Field(text, len, "sig=", &fsig, &sigLen) ||
        idLen != LICENSE_ID_LEN - 1)
        return LICENSE_NONE;
    char msg[64] = "VestaHome support 1|";
    msg[18] = (char)('0' + level);   // (signed with the ID: a level changed in the file breaks the signature)
    int m = (int)strlen(msg);
    for (int i = 0; i < idLen; i++) {
        if ((wchar_t)(unsigned char)fid[i] != id[i])
            return LICENSE_NONE;   // (another unit's file)
        msg[m++] = fid[i];
    }
    unsigned char sig[64];
    if (Base64(fsig, sigLen, sig, sizeof(sig)) != 64)
        return LICENSE_NONE;
    unsigned char h[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, msg, m);
    br_sha256_out(&c, h);
    br_ec_public_key pk;
    pk.curve = BR_EC_secp256r1;
    pk.q = (unsigned char*)pub;
    pk.qlen = publen;
    return br_ecdsa_i31_vrfy_raw(&br_ec_p256_m31, h, sizeof(h), &pk, sig, sizeof(sig)) == 1 ? level : LICENSE_NONE;
}

int LicenseValid(const char* text, int len, const wchar_t* id)
{
    return LicenseVerify(text, len, id, kLicensePub, sizeof(kLicensePub));
}

int LicenseFileVerify(const wchar_t* path, const wchar_t* id, const unsigned char* pub, int publen, bool* found)
{
    char text[1024];
    DWORD got = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (found)
        *found = f != INVALID_HANDLE_VALUE;
    if (f == INVALID_HANDLE_VALUE)
        return LICENSE_NONE;
    ReadFile(f, text, sizeof(text), &got, NULL);
    CloseHandle(f);
    return LicenseVerify(text, (int)got, id, pub, publen);
}

int LicenseFileValid(const wchar_t* path, const wchar_t* id, bool* found)
{
    return LicenseFileVerify(path, id, kLicensePub, sizeof(kLicensePub), found);
}

void LicenseKeyBeside(const wchar_t* program, wchar_t* path)
{
    int n = (int)wcslen(program);
    while (n && program[n - 1] != L'\\')
        n--;
    wcsncpy(path, program, n);
    wcscpy(path + n, L"VestaLicense.key");
}

int LicenseUnitLevel(wchar_t* id)
{
    LicenseUnitId(id);
    wchar_t self[MAX_PATH], path[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    LicenseKeyBeside(self, path);
    int home = LicenseFileValid(L"\\ResidentFlash\\VestaHome\\VestaLicense.key", id, NULL);
    int own = LicenseFileValid(path, id, NULL);
    return home > own ? home : own;
}

#ifdef UNDER_CE
extern "C" BOOL KernelIoControl(DWORD code, LPVOID in, DWORD inSize, LPVOID out, DWORD outSize, LPDWORD ret);

static int MacBytes(unsigned char* buf, int cap)   // the adapters' MAC addresses, one after another
{
    struct IpAddr { void* next; char ip[16]; char mask[16]; DWORD ctx; };
    struct Adapter { Adapter* next; DWORD combo; char name[260]; char descr[132]; UINT alen; BYTE addr[8]; };
    typedef DWORD (WINAPI *AdaptersFn)(void*, ULONG*);
    HMODULE ip = LoadLibrary(L"iphlpapi.dll");
    AdaptersFn f = ip ? (AdaptersFn)GetProcAddress(ip, L"GetAdaptersInfo") : NULL;
    ULONG size = 0;
    int n = 0;
    if (f)
        f(NULL, &size);
    if (f && size && size < 64 * 1024) {
        Adapter* a = (Adapter*)malloc(size);
        if (a && f(a, &size) == 0)
            for (Adapter* p = a; p && n + 8 <= cap; p = p->next)
                if (p->alen >= 6 && p->alen <= 8) {
                    memcpy(buf + n, p->addr, p->alen);
                    n += p->alen;
                }
        free(a);
    }
    return n;
}
#endif

int LicenseUnitBytes(unsigned char* buf, int cap, int* source)
{
    int n, src;
#ifdef UNDER_CE
    const DWORD IOCTL_HAL_GET_DEVICEID = 0x01010054;   // CTL_CODE(FILE_DEVICE_HAL, 21, METHOD_BUFFERED, 0)
    DWORD got = 0;
    memset(buf, 0, cap);
    *(DWORD*)buf = cap;   // DEVICE_ID.dwSize
    if (KernelIoControl(IOCTL_HAL_GET_DEVICEID, NULL, 0, buf, cap, &got) && got > 20) {
        n = (int)got;
        src = LICENSE_SRC_HAL;
    } else {
        n = MacBytes(buf, cap);
        src = n ? LICENSE_SRC_MAC : LICENSE_SRC_NONE;
    }
#else
    DWORD serial = 0;   // PC test build: the system volume's serial number
    GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    memcpy(buf, &serial, 4);
    n = 4;
    src = LICENSE_SRC_PC;
#endif
    if (source)
        *source = src;
    return n;
}

void LicenseUnitId(wchar_t* out)
{
    unsigned char hw[LICENSE_UNIT_MAX];
    LicenseIdText(hw, LicenseUnitBytes(hw, sizeof(hw), NULL), out);
}

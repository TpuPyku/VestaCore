// vlicense.cpp - VestaHome's support license (vlicense.h): the unit's ID text, the license file checked by BearSSL.
#include "vlicense.h"
#include <string.h>
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

bool LicenseVerify(const char* text, int len, const wchar_t* id, const unsigned char* pub, int publen)
{
    const char *fid, *fsig;
    int idLen, sigLen;
    if (!Field(text, len, "id=", &fid, &idLen) || !Field(text, len, "sig=", &fsig, &sigLen) ||
        idLen != LICENSE_ID_LEN - 1)
        return false;
    char msg[64] = "VestaHome support 1|";
    int m = (int)strlen(msg);
    for (int i = 0; i < idLen; i++) {
        if ((wchar_t)(unsigned char)fid[i] != id[i])
            return false;   // (another unit's file)
        msg[m++] = fid[i];
    }
    unsigned char sig[64];
    if (Base64(fsig, sigLen, sig, sizeof(sig)) != 64)
        return false;
    unsigned char h[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, msg, m);
    br_sha256_out(&c, h);
    br_ec_public_key pk;
    pk.curve = BR_EC_secp256r1;
    pk.q = (unsigned char*)pub;
    pk.qlen = publen;
    return br_ecdsa_i31_vrfy_raw(&br_ec_p256_m31, h, sizeof(h), &pk, sig, sizeof(sig)) == 1;
}

bool LicenseValid(const char* text, int len, const wchar_t* id)
{
    return LicenseVerify(text, len, id, kLicensePub, sizeof(kLicensePub));
}

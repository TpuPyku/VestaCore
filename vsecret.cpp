// vsecret.cpp - VestaCore: secrets sealed with a key of this head unit (see vsecret.h).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vsecret.h"
#include "vlicense.h"

extern "C" {
#include "bearssl.h"
}

enum { kNonce = 12, kTag = 8, kMaxPlain = 128 };

struct Key {
    unsigned char enc[32];   // ChaCha20 key
    unsigned char mac[32];   // HMAC key
    bool          ready;
};
// The key of the machine ID (vlicense.h LicenseUnitBytes) and the old one (2026-11-25: the HAL's device id - the same
// on every unit; what it sealed still opens, SecretNeedsReseal tells to seal it again). With no machine ID yet the old
// one seals too (the caller seals again once it is there).
static Key s_cur, s_old;
static int s_source;   // LICENSE_SRC_* of s_cur

static void MakeKey(const unsigned char* id, int n, Key* key)
{
    br_sha256_context h;
    unsigned char k[32];
    br_sha256_init(&h);
    br_sha256_update(&h, "VestaHome secret key v1", 23);
    br_sha256_update(&h, id, n);
    br_sha256_out(&h, k);
    br_sha256_init(&h);   // two keys from it
    br_sha256_update(&h, k, 32);
    br_sha256_update(&h, "enc", 3);
    br_sha256_out(&h, key->enc);
    br_sha256_init(&h);
    br_sha256_update(&h, k, 32);
    br_sha256_update(&h, "mac", 3);
    br_sha256_out(&h, key->mac);
    key->ready = n > 0;
}

int SecretInit()
{
    unsigned char id[LICENSE_UNIT_MAX];   // (LicenseLegacyBytes: the HAL's answer depends on the room - as it always was)
    MakeKey(id, LicenseUnitBytes(id, sizeof(id), &s_source), &s_cur);
    MakeKey(id, LicenseLegacyBytes(id, sizeof(id)), &s_old);
    return s_source;
}

static const Key* SealKey()
{
    return s_cur.ready ? &s_cur : s_old.ready ? &s_old : NULL;
}

static void Tag(const Key* key, const unsigned char* data, int len, unsigned char* out)
{
    br_hmac_key_context kc;
    br_hmac_context hc;
    br_hmac_key_init(&kc, &br_sha256_vtable, key->mac, 32);
    br_hmac_init(&hc, &kc, kTag);
    br_hmac_update(&hc, data, len);
    br_hmac_out(&hc, out);
}

// A nonce that never repeats for this key: the clock, the counters and a sequence through SHA-256.
static void Nonce(const Key* key, unsigned char* out)
{
    static DWORD s_seq;
    struct { DWORD tick, seq, thread; LARGE_INTEGER perf; SYSTEMTIME t; } src;
    src.tick = GetTickCount();
    src.seq = ++s_seq;
    src.thread = GetCurrentThreadId();
    QueryPerformanceCounter(&src.perf);
    GetSystemTime(&src.t);
    unsigned char h[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, &src, sizeof(src));
    br_sha256_update(&c, key->enc, 32);
    br_sha256_out(&c, h);
    memcpy(out, h, kNonce);
}

// sealed = nonce, cipher text (n bytes), tag: which key's it is (NULL - none's: another unit's, or damaged).
static const Key* KeyOf(const unsigned char* sealed, int n)
{
    const Key* keys[2] = { &s_cur, &s_old };
    for (int i = 0; i < 2; i++) {
        unsigned char tag[kTag];
        if (!keys[i]->ready)
            continue;
        Tag(keys[i], sealed, kNonce + n, tag);
        if (!memcmp(tag, sealed + kNonce + n, kTag))
            return keys[i];
    }
    return NULL;
}

static void SealIn(const Key* key, unsigned char* buf, int n)   // buf + kNonce: the plain n bytes; room for the tag
{
    Nonce(key, buf);
    br_chacha20_ct_run(key->enc, buf, 1, buf + kNonce, n);
    Tag(key, buf, kNonce + n, buf + kNonce + n);
}

bool SecretIsSealed(const wchar_t* s)
{
    return !wcsncmp(s, L"v1:", 3);
}

bool SecretSeal(const wchar_t* plain, wchar_t* out, int cap)
{
    unsigned char buf[kNonce + kMaxPlain + kTag];
    int n = WideCharToMultiByte(CP_UTF8, 0, plain, -1, (char*)buf + kNonce, kMaxPlain, NULL, NULL) - 1;
    int total = kNonce + n + kTag;
    const Key* key = SealKey();
    if (!key || n < 0 || cap < 4 + total * 2)
        return false;
    SealIn(key, buf, n);
    wcscpy(out, L"v1:");
    for (int i = 0; i < total; i++)
        _snwprintf(out + 3 + i * 2, 3, L"%02x", buf[i]);
    out[3 + total * 2] = 0;
    return true;
}

int SecretSealBytes(const unsigned char* plain, int n, unsigned char* out, int cap)
{
    int total = kNonce + n + kTag;
    const Key* key = SealKey();
    if (!key || n < 0 || cap < total)
        return -1;
    memcpy(out + kNonce, plain, n);
    SealIn(key, out, n);
    return total;
}

int SecretOpenBytes(const unsigned char* sealed, int len, unsigned char* out, int cap)
{
    int n = len - kNonce - kTag;
    const Key* key = n >= 0 && cap >= n ? KeyOf(sealed, n) : NULL;
    if (!key)
        return -1;   // another unit's key, damaged, or out too small
    memcpy(out, sealed + kNonce, n);
    br_chacha20_ct_run(key->enc, sealed, 1, out, n);
    return n;
}

// The text's bytes opened in buf: which key's (NULL - not sealed here, or damaged); n - the plain length.
static const Key* OpenText(const wchar_t* sealed, unsigned char* buf, int* n)
{
    if (!SecretIsSealed(sealed))
        return NULL;
    const wchar_t* h = sealed + 3;
    int total = (int)wcslen(h) / 2;
    if (total < kNonce + kTag || total > kNonce + kMaxPlain + kTag)
        return NULL;
    for (int i = 0; i < total; i++) {
        unsigned v;
        if (swscanf(h + i * 2, L"%2x", &v) != 1)
            return NULL;
        buf[i] = (unsigned char)v;
    }
    *n = total - kNonce - kTag;
    const Key* key = KeyOf(buf, *n);
    if (key)
        br_chacha20_ct_run(key->enc, buf, 1, buf + kNonce, *n);
    return key;
}

bool SecretOpen(const wchar_t* sealed, wchar_t* plain, int cap)
{
    unsigned char buf[kNonce + kMaxPlain + kTag + 1];
    int n = 0;
    plain[0] = 0;
    if (!OpenText(sealed, buf, &n))
        return false;
    buf[kNonce + n] = 0;
    return MultiByteToWideChar(CP_UTF8, 0, (char*)buf + kNonce, -1, plain, cap) > 0;
}

bool SecretNeedsReseal(const wchar_t* sealed)
{
    unsigned char buf[kNonce + kMaxPlain + kTag + 1];
    int n = 0;
    const Key* key = OpenText(sealed, buf, &n);
    return key && key != SealKey();
}

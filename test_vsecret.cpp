// test_vsecret.cpp - the secrets sealed with this unit's key (vsecret.h) on the PC (the key: the PC's volume).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vsecret.h"
#include "vlicense.h"

static int s_fail;

static void Check(bool ok, const char* what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        s_fail++;
    }
}

static void TestText()
{
    wchar_t sealed[400], plain[80];
    Check(!SecretSeal(L"x", sealed, 400) && !SecretOpen(L"v1:00", plain, 80), "text: no key before init - nothing");
    Check(SecretInit() == 3, "text: the PC's key");
    Check(SecretSeal(L"пароль 1=2", sealed, 400) && SecretIsSealed(sealed), "text: sealed, v1:");
    Check(wcsstr(sealed, L"пароль") == NULL, "text: no plain text in it");
    Check(SecretOpen(sealed, plain, 80) && !wcscmp(plain, L"пароль 1=2"), "text: opened back");
    wchar_t again[400];
    Check(SecretSeal(L"пароль 1=2", again, 400) && wcscmp(again, sealed), "text: a new nonce each time");
    sealed[10] = sealed[10] == L'0' ? L'1' : L'0';
    Check(!SecretOpen(sealed, plain, 80) && !plain[0], "text: a changed byte - refused, empty");
    Check(!SecretOpen(L"plain", plain, 80), "text: not sealed - refused");
    Check(!SecretIsSealed(L"plain") && SecretIsSealed(L"v1:"), "text: the mark");
    Check(!SecretSeal(L"abc", sealed, 30), "text: out too small - false");
    Check(SecretSeal(L"abc", sealed, 4 + (12 + 3 + 8) * 2) && SecretOpen(sealed, plain, 80) && !wcscmp(plain, L"abc"),
          "text: out exactly big enough");
    wchar_t lng[200];
    for (int i = 0; i < 199; i++)
        lng[i] = L'a';
    lng[199] = 0;
    Check(!SecretSeal(lng, sealed, 400), "text: longer than 127 bytes - false");
    lng[127] = 0;
    Check(SecretSeal(lng, sealed, 400) && SecretOpen(sealed, plain, 80) == false, "text: plain bigger than out - false");
    wchar_t big[200];
    Check(SecretOpen(sealed, big, 200) && !wcscmp(big, lng), "text: 127 bytes - the most");
}

static void TestBytes()   // (the phone book's cache)
{
    unsigned char plain[300], sealed[400], back[400];
    for (int i = 0; i < 300; i++)
        plain[i] = (unsigned char)(i * 7);
    int n = SecretSealBytes(plain, 300, sealed, sizeof(sealed));
    Check(n > 300 && n < 400, "seal: nonce and tag added");
    Check(memcmp(sealed + 12, plain, 300) != 0, "seal: not plain");
    Check(SecretSealBytes(plain, 300, sealed, n - 1) < 0, "seal: too small - -1");
    n = SecretSealBytes(plain, 300, sealed, sizeof(sealed));
    Check(SecretOpenBytes(sealed, n, back, sizeof(back)) == 300 && !memcmp(back, plain, 300), "open: the same back");
    Check(SecretOpenBytes(sealed, n, back, 299) < 0, "open: too small - -1");
    Check(SecretOpenBytes(sealed, n, back, 300) == 300, "open: just big enough");
    Check(SecretSealBytes(plain, 300, sealed, n) == n, "seal: just big enough");
    Check(SecretSealBytes(plain, -1, sealed, sizeof(sealed)) < 0, "seal: a negative length - -1");
    int e = SecretSealBytes(plain, 0, sealed, sizeof(sealed));
    Check(e > 0 && SecretOpenBytes(sealed, e, back, 0) == 0, "seal: nothing - nonce and tag, opened back to nothing");
    static unsigned char bigPlain[20000], bigSealed[20100], bigBack[20000];   // (a phone book of hundreds of names)
    bigPlain[19999] = 7;
    int bn = SecretSealBytes(bigPlain, 20000, bigSealed, sizeof(bigSealed));
    Check(bn == 20020 && SecretOpenBytes(bigSealed, bn, bigBack, sizeof(bigBack)) == 20000 && bigBack[19999] == 7,
          "seal / open: 20 000 bytes");
    sealed[20] ^= 1;
    Check(SecretOpenBytes(sealed, n, back, sizeof(back)) < 0, "open: damaged (or another unit's) - -1");
    Check(SecretOpenBytes(sealed, 10, back, sizeof(back)) < 0, "open: shorter than nonce and tag - -1");
}

// The key of the machine ID (2026-11-25: the HAL's device id is the same on every unit): what the old key (the HAL's,
// here the PC's volume) sealed still opens and asks to be sealed again; another unit's machine ID - not opened.
static void TestMachineKey()
{
    wchar_t s1[400], s2[400], plain[80];
    unsigned char raw[3] = { 1, 2, 3 }, b1[64], back[64];
    LicenseMachineSet(NULL);
    Check(SecretInit() == 3, "machine: none - the PC's key");
    Check(SecretSeal(L"old", s1, 400), "machine: sealed with the old key");
    int n1 = SecretSealBytes(raw, 3, b1, sizeof(b1));
    Check(!SecretNeedsReseal(s1), "machine: the current key's - nothing to do");
    LicenseMachineSet(L"41800514 180624 093151");
    Check(SecretInit() == 4, "machine: the MCU's");
    Check(SecretOpen(s1, plain, 80) && !wcscmp(plain, L"old"), "machine: the old key's opened");
    Check(SecretNeedsReseal(s1), "machine: the old key's - to be sealed again");
    Check(SecretOpenBytes(b1, n1, back, sizeof(back)) == 3 && !memcmp(back, raw, 3), "machine: the old key's bytes too");
    Check(SecretSeal(L"new", s2, 400) && !SecretNeedsReseal(s2), "machine: sealed with the new key");
    Check(SecretOpen(s2, plain, 80) && !wcscmp(plain, L"new"), "machine: and opened");
    Check(!SecretNeedsReseal(L"plain") && !SecretNeedsReseal(L"v1:00"), "machine: not sealed or damaged - not to reseal");
    Check(SecretSeal(L"", s2, 400) && SecretOpen(s2, plain, 80) && !plain[0], "machine: an empty one sealed and opened");
    LicenseMachineSet(L"41800514 180624 149000");
    SecretInit();
    Check(!SecretOpen(s2, plain, 80), "machine: another unit's - not opened");
    // What a unit sealed stays readable after an update: the key made the same way (fixed vectors, 2026-11-25)
    LicenseMachineSet(L"KAT 000000 TEST");
    SecretInit();
    Check(SecretOpen(L"v1:a2592bf7eb0f50219a22a5d067d8e1b0dde47d425b3af7123e", plain, 80) && !wcscmp(plain, L"vesta"),
          "machine: a password sealed before - opened");
    static const unsigned char kKat[] = { 0x3c, 0x2c, 0x0a, 0xb8, 0x8b, 0x4e, 0xe2, 0xa6, 0x48, 0x94, 0x5c, 0x02, 0xff,
                                          0xba, 0xc3, 0x1c, 0xbb, 0x23, 0xa3, 0xe6, 0x88, 0x4a, 0x91 };
    Check(SecretOpenBytes(kKat, sizeof(kKat), back, sizeof(back)) == 3 && !memcmp(back, raw, 3),
          "machine: bytes sealed before - opened");
    LicenseMachineSet(NULL);
}

int main()
{
    wchar_t kept[64] = L"";   // (the PC build's demo machine ID: put back at the end)
    LicenseMachineGet(kept, 64);
    LicenseMachineSet(NULL);
    TestText();
    TestBytes();
    TestMachineKey();
    if (kept[0])
        LicenseMachineSet(kept);
    printf(s_fail ? "%d FAILED\n" : "test_vsecret: all passed\n", s_fail);
    return s_fail;
}

// test_vsecret.cpp - the secrets sealed with this unit's key (vsecret.h) on the PC (the key: the PC's volume).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vsecret.h"

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
    sealed[20] ^= 1;
    Check(SecretOpenBytes(sealed, n, back, sizeof(back)) < 0, "open: damaged (or another unit's) - -1");
    Check(SecretOpenBytes(sealed, 10, back, sizeof(back)) < 0, "open: shorter than nonce and tag - -1");
}

int main()
{
    TestText();
    TestBytes();
    printf(s_fail ? "%d FAILED\n" : "test_vsecret: all passed\n", s_fail);
    return s_fail;
}

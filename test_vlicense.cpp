// test_vlicense.cpp - vlicense.cpp on the PC: the unit's ID text, a license file checked against a test key
// (tools/testdata/license_test_priv.pem - for these tests only; its files made with tools/make_license.py sign).
#include "vlicense.h"
#include <stdio.h>
#include <string.h>

static int s_checks, s_failed;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

static const unsigned char kTestPub[65] = {   // the test key's public half (make_license.py keygen)
    0x04, 0x56, 0x87, 0xCE, 0x21, 0xCB, 0x7B, 0x24, 0xBD, 0x50, 0x50, 0x20, 0x69,
    0x12, 0x91, 0x5B, 0x2A, 0x7B, 0xD6, 0x9C, 0xEA, 0x26, 0x0C, 0x51, 0x62, 0x01,
    0x3C, 0xAD, 0x6B, 0x5C, 0xED, 0xF2, 0x3A, 0xE1, 0x3E, 0x04, 0x64, 0xBC, 0xCA,
    0xCA, 0xF1, 0x39, 0xF0, 0xEA, 0x9F, 0x4F, 0x32, 0x95, 0xA8, 0x91, 0x85, 0x5D,
    0x01, 0x2B, 0x98, 0x1D, 0x29, 0xC5, 0x5E, 0x81, 0x68, 0x09, 0x50, 0x54, 0x16,
};
static const char kFile[] = "VestaHome support license\nid=VH-ABCDE-FGH12\n"
                            "sig=cRR+aQwqOWLY9NnbjMqsu1+aRGUN5L9GN4wxgLUbqRVNB3ATWeJCz5e2O+aEMQMNyzwEigaAHTRaZQGqraj8ig==\n";

static bool Verify(const char* text, const wchar_t* id)
{
    return LicenseVerify(text, (int)strlen(text), id, kTestPub, sizeof(kTestPub));
}

static void TestId()
{
    unsigned char hw[21];
    for (int i = 0; i < 21; i++)
        hw[i] = (unsigned char)i;
    wchar_t id[LICENSE_ID_LEN];
    LicenseIdText(hw, 21, id);
    Check(!wcscmp(id, L"VH-EPQEK-Q69ZF"), "the ID: SHA-256 of the unit's bytes, 50 bits in 10 letters (as make_license.py id)");
    hw[20] ^= 1;
    wchar_t other[LICENSE_ID_LEN];
    LicenseIdText(hw, 21, other);
    Check(wcscmp(id, other) != 0, "another unit - another ID");
}

static void TestFile()
{
    Check(Verify(kFile, L"VH-ABCDE-FGH12"), "the author's file for this ID: valid");
    Check(!Verify(kFile, L"VH-ABCDE-FGH13"), "the same file on another unit: not valid");
    char crlf[300] = "VestaHome support license\r\nid=VH-ABCDE-FGH12\r\nsig=cRR+aQwqOWLY9NnbjMqsu1+aRGUN5L9GN4wxgLUbqRVNB3ATW"
                     "eJCz5e2O+aEMQMNyzwEigaAHTRaZQGqraj8ig==\r\n";
    Check(Verify(crlf, L"VH-ABCDE-FGH12"), "saved by Notepad (CRLF): still valid");
    char bad[300];
    strcpy(bad, kFile);
    char* s = strstr(bad, "sig=") + 10;
    *s = *s == 'A' ? 'B' : 'A';
    Check(!Verify(bad, L"VH-ABCDE-FGH12"), "a signature's letter changed: not valid");
    strcpy(bad, kFile);
    memcpy(strstr(bad, "id=") + 3, "VH-ABCDE-FGH13", 14);
    Check(!Verify(bad, L"VH-ABCDE-FGH13"), "the ID in the file changed to this unit's: the signature does not match");
    Check(!Verify("VestaHome support license\nid=VH-ABCDE-FGH12\n", L"VH-ABCDE-FGH12"), "no signature: not valid");
    Check(!Verify("sig=cRR+aQ==\nid=VH-ABCDE-FGH12\n", L"VH-ABCDE-FGH12"), "a short signature: not valid");
    Check(!Verify("", L"VH-ABCDE-FGH12") && !Verify("junk", L"VH-ABCDE-FGH12"), "empty, junk: not valid");
    Check(!LicenseValid(kFile, (int)strlen(kFile), L"VH-ABCDE-FGH12"), "the test key's file under the real key: not valid");
    // (the mutator's survivors: the id on the first line; a signature with '0', '/' and '+' - each letter decoded)
    Check(Verify("id=VH-0Z9Y8-X7W6V\nsig=epTtf7hbvNb913Byulc7DIURy7HEbHhFEJJ9u96UwhJPjLeRfme/IGunPuO0eo3xdc3cXK2/HuhMT+Tm"
                 "Cyb23g==", L"VH-0Z9Y8-X7W6V"), "no title line, the id first, no line end at the close: valid");
    Check(!LicenseVerify(kFile, 40, L"VH-ABCDE-FGH12", kTestPub, sizeof(kTestPub)),
          "only the given length read (the signature cut off): not valid");
}

int main()
{
    TestId();
    TestFile();
    printf("test_vlicense: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

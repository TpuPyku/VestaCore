// test_vlicense.cpp - vlicense.cpp on the PC: the unit's ID text, a license file checked against a test key
// (tools/testdata/license_test_priv.pem - for these tests only; its files made with tools/make_license.py sign).
#include "vlicense.h"
#include <windows.h>
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

static int Level(const char* text)
{
    return LicenseVerify(text, (int)strlen(text), L"VH-ABCDE-FGH12", kTestPub, sizeof(kTestPub));
}

static void TestLevels()   // a subscriber (1) or a co-author (2), the level signed with the ID (2026-11-18)
{
    static const char kCoauthor[] = "VestaHome support license\nid=VH-ABCDE-FGH12\nlevel=2\n"
        "sig=D1XfibHg2sYVLgSI1i+FcZWZLbJLLLQvX3crMy1fwzMAjb7XCjNkziYd4DlSFFqgEtL4iamB+7GcVvGGL45OcA==\n";
    Check(Level(kFile) == LICENSE_SUPPORTER, "a key without a level (all made before levels): a subscriber");
    Check(Level(kCoauthor) == LICENSE_COAUTHOR, "a co-author's key: level 2");
    char t[300];
    strcpy(t, kCoauthor);
    *strstr(t, "level=2") = '\0';
    strcat(t, strstr(kCoauthor, "sig="));
    Check(Level(t) == LICENSE_NONE, "the level line taken out of a co-author's key: not valid");
    strcpy(t, kCoauthor);
    strstr(t, "level=2")[6] = '1';
    Check(Level(t) == LICENSE_NONE, "level=2 turned into 1: not valid");
    strcpy(t, kFile);
    *strstr(t, "sig=") = '\0';
    strcat(t, "level=2\n");
    strcat(t, strstr(kFile, "sig="));
    Check(Level(t) == LICENSE_NONE, "level=2 added to a subscriber's key: not valid");
    strcpy(t, kCoauthor);
    strstr(t, "level=2")[6] = 'x';
    Check(Level(t) == LICENSE_NONE, "a level not a digit: not valid");
    strcpy(t, kCoauthor);
    strcpy(strstr(t, "level=2"), "level=22\n");
    strcat(t, strstr(kCoauthor, "sig="));
    Check(Level(t) == LICENSE_NONE, "a level of two digits: not valid");
    strcpy(t, kFile);
    *strstr(t, "sig=") = '\0';
    strcat(t, "level=1\n");
    strcat(t, strstr(kFile, "sig="));
    Check(Level(t) == LICENSE_SUPPORTER, "level=1 written out in a subscriber's key: still a subscriber");
    strcpy(t, kCoauthor);
    strstr(t, "level=2")[6] = '3';
    Check(Level(t) == LICENSE_NONE, "an unknown level: not valid");
}

static void TestUnit()   // (the PC: the system volume's serial number; the unit: its HAL device id, else MACs)
{
    unsigned char a[64], b[64];
    int source = -1;
    int n = LicenseUnitBytes(a, sizeof(a), &source);
    Check(n == 4 && source == LICENSE_SRC_PC && LicenseUnitBytes(b, sizeof(b), NULL) == 4 && !memcmp(a, b, 4),
          "the unit's bytes: the same each time, their source told");
    wchar_t id[LICENSE_ID_LEN], same[LICENSE_ID_LEN];
    LicenseIdText(a, n, same);
    LicenseUnitId(id);
    Check(!wcscmp(id, same) && !wcsncmp(id, L"VH-", 3), "this unit's ID: the text of its bytes");
}

static void TestPath()   // a license file read from its path (VestaHome's folder; VestaAuto reads it itself)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    _snwprintf(path, MAX_PATH, L"%svlicense_test.key", dir);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD put;
    WriteFile(f, kFile, (DWORD)strlen(kFile), &put, NULL);
    CloseHandle(f);
    bool found = false;
    Check(LicenseFileVerify(path, L"VH-ABCDE-FGH12", kTestPub, sizeof(kTestPub), &found) && found,
          "the file at its path: found, valid");
    found = true;
    Check(!LicenseFileVerify(path, L"VH-ABCDE-FGH13", kTestPub, sizeof(kTestPub), &found) && found,
          "found, another unit's: not valid");
    Check(!LicenseFileValid(path, L"VH-ABCDE-FGH12", &found) && found, "the real key: the test file not valid");
    DeleteFileW(path);
    found = true;
    Check(!LicenseFileVerify(path, L"VH-ABCDE-FGH12", kTestPub, sizeof(kTestPub), &found) && !found,
          "no file: not valid, not found");
    Check(!LicenseFileValid(path, L"VH-ABCDE-FGH12", NULL), "no file, no `found` asked: not valid");
}

int main()
{
    TestId();
    TestFile();
    TestLevels();
    TestUnit();
    TestPath();
    printf("test_vlicense: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

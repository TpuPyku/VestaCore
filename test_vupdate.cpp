// test_vupdate.cpp - vupdate.cpp on the PC: the installer's version mark, its signature (a test key's), the versions
// compared, the boot job's text.
#include "vupdate.h"
#include <bearssl.h>
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

static const unsigned char kTestPub[65] = {   // tools/testdata/license_test_priv.pem's public half
    0x04, 0x56, 0x87, 0xCE, 0x21, 0xCB, 0x7B, 0x24, 0xBD, 0x50, 0x50, 0x20, 0x69,
    0x12, 0x91, 0x5B, 0x2A, 0x7B, 0xD6, 0x9C, 0xEA, 0x26, 0x0C, 0x51, 0x62, 0x01,
    0x3C, 0xAD, 0x6B, 0x5C, 0xED, 0xF2, 0x3A, 0xE1, 0x3E, 0x04, 0x64, 0xBC, 0xCA,
    0xCA, 0xF1, 0x39, 0xF0, 0xEA, 0x9F, 0x4F, 0x32, 0x95, 0xA8, 0x91, 0x85, 0x5D,
    0x01, 0x2B, 0x98, 0x1D, 0x29, 0xC5, 0x5E, 0x81, 0x68, 0x09, 0x50, 0x54, 0x16,
};
// An installer stand-in and its signature (make_license.py sign-installer with the test key)
static const char kExe[] = "MZ-test-installer:VHINSTVER:1.1.261130\0payload";
static const char kSig[] = "VestaHome installer signature\nversion=1.1.261130\n"
                           "sha256=3595863107a3c21a4c4df6e29e61e32892ed516219bb40b73381364903895841\n"
                           "sig=PSvjoDKfv66HmhvLLv1tvG2x0gMyMXrSGvu73oVmYjeRLeUCQR3KjE6oyyyRWNq2Z7yOddqNg5poDgIv/PWEAw==\n";

static wchar_t s_dir[MAX_PATH];

static void Put(const wchar_t* name, const void* b, int n, wchar_t* path)
{
    _snwprintf(path, MAX_PATH, L"%s%s", s_dir, name);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w;
    WriteFile(f, b, n, &w, NULL);
    CloseHandle(f);
}

static void TestMark()
{
    char v[32];
    Check(UpdateMark((const unsigned char*)kExe, sizeof(kExe) - 1, v, 32) && !strcmp(v, "1.1.261130"), "mark: found");
    Check(!UpdateMark((const unsigned char*)"MZ nothing", 10, v, 32) && !v[0], "mark: none");
    Check(!UpdateMark((const unsigned char*)"VHINSTVER:1.2", 13, v, 32), "mark: no end - none");
    Check(!UpdateMark((const unsigned char*)"VHINSTVER:\0", 11, v, 32), "mark: empty - none");
    Check(!UpdateMark((const unsigned char*)"VHINSTVER:1.1.261130\0", 21, v, 8), "mark: longer than the room - none");
    Check(UpdateMark((const unsigned char*)"VHINSTVER:1.1.261130\0", 21, v, 11) && !strcmp(v, "1.1.261130"),
          "mark: just the room");
    Check(!UpdateMark((const unsigned char*)"VHINSTVER:1.1.261130\0", 21, v, 10), "mark: a byte short");
    static const char two[] = "VESTA VHINSTVER:\0 VestaHome.exe inside; VHINSTVER:1.2.3\0";
    Check(UpdateMark((const unsigned char*)two, sizeof(two) - 1, v, 32) && !strcmp(v, "1.2.3"),
          "mark: VestaHome's empty one inside the installer passed over, the installer's own found");
    Check(UpdateMark((const unsigned char*)"VHINSTVER:9\0", 12, v, 32) && !strcmp(v, "9"), "mark: at the very start");
}

static void TestCompare()
{
    Check(UpdateVersionCmp("1.1.261130", "1.1.261125") > 0, "versions: a later date");
    Check(UpdateVersionCmp("1.1.261125", "1.1.261125") == 0, "versions: the same");
    Check(UpdateVersionCmp("1.0.261201", "1.1.261125") < 0, "versions: the release first");
    Check(UpdateVersionCmp("1.10.1", "1.9.1") > 0, "versions: numbers, not text");
    Check(UpdateVersionCmp("1.1", "1.1.0") < 0 && UpdateVersionCmp("1.1.0", "1.1") > 0, "versions: a part more - later");
}

static void TestCheck()
{
    wchar_t exe[MAX_PATH], sig[MAX_PATH];
    char v[32];
    unsigned char sha[32];
    Put(L"vu_inst.exe", kExe, sizeof(kExe) - 1, exe);
    Put(L"vu_inst.sig", kSig, sizeof(kSig) - 1, sig);
    Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_OK && !strcmp(v, "1.1.261130") &&
          sha[0] == 0x35 && sha[31] == 0x41, "check: signed - its version and SHA-256");
    Check(UpdateCheck(exe, sig, NULL, 0, v, 32, sha) == UPDATE_BAD_SIG, "check: the author's key - not the test one's");
    wchar_t none[MAX_PATH];
    _snwprintf(none, MAX_PATH, L"%svu_none.sig", s_dir);
    Check(UpdateCheck(exe, none, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_NO_SIG && !strcmp(v, "1.1.261130"),
          "check: no signature file - told, the version still read");
    _snwprintf(none, MAX_PATH, L"%svu_none.exe", s_dir);
    Check(UpdateCheck(none, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_NO_FILE, "check: no installer");
    char other[sizeof(kExe)];
    memcpy(other, kExe, sizeof(kExe));
    other[2] = 'X';   // a changed byte: another SHA-256
    Put(L"vu_inst.exe", other, sizeof(kExe) - 1, exe);
    Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_BAD_SIG, "check: a changed file - refused");
    Put(L"vu_inst.exe", "MZ no mark", 10, exe);
    Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_NO_MARK, "check: no mark - not ours");
    char sig2[sizeof(kSig)];
    memcpy(sig2, kSig, sizeof(kSig));
    char* at = strstr(sig2, "261130\nsha");
    at[5] = '1';   // the signed version changed
    Put(L"vu_inst.exe", kExe, sizeof(kExe) - 1, exe);
    Put(L"vu_inst.sig", sig2, sizeof(kSig) - 1, sig);
    Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_BAD_SIG,
          "check: another version in the signature file - refused");
    static const char* kBroken[3] = {   // a line missing: refused
        "VestaHome installer signature\nsha256=3595863107a3c21a4c4df6e29e61e32892ed516219bb40b73381364903895841\n"
        "sig=PSvjoDKfv66HmhvLLv1tvG2x0gMyMXrSGvu73oVmYjeRLeUCQR3KjE6oyyyRWNq2Z7yOddqNg5poDgIv/PWEAw==\n",
        "VestaHome installer signature\nversion=1.1.261130\n"
        "sig=PSvjoDKfv66HmhvLLv1tvG2x0gMyMXrSGvu73oVmYjeRLeUCQR3KjE6oyyyRWNq2Z7yOddqNg5poDgIv/PWEAw==\n",
        "VestaHome installer signature\nversion=1.1.261130\n"
        "sha256=3595863107a3c21a4c4df6e29e61e32892ed516219bb40b73381364903895841\n" };
    for (int i = 0; i < 3; i++) {
        Put(L"vu_inst.sig", kBroken[i], (int)strlen(kBroken[i]), sig);
        Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_BAD_SIG, "check: a line missing");
    }
    static char big[200000];   // the mark far in, across the reading's chunks
    memset(big, 'a', sizeof(big));
    memcpy(big + 65530, "VHINSTVER:2.0.1", 15);
    big[65545] = 0;
    Put(L"vu_big.exe", big, sizeof(big), exe);
    Check(UpdateCheck(exe, sig, kTestPub, sizeof(kTestPub), v, 32, sha) == UPDATE_BAD_SIG && !strcmp(v, "2.0.1"),
          "check: a mark across two chunks found");
    unsigned char want[32];
    br_sha256_context c;
    br_sha256_init(&c);
    br_sha256_update(&c, big, sizeof(big));
    br_sha256_out(&c, want);
    Check(!memcmp(sha, want, 32), "check: the SHA-256 of all its chunks");
    DeleteFileW(exe);
    _snwprintf(exe, MAX_PATH, L"%svu_inst.exe", s_dir);
    DeleteFileW(exe);
    DeleteFileW(sig);
}

static void TestJob()
{
    unsigned char sha[32], back[32];
    for (int i = 0; i < 32; i++)
        sha[i] = (unsigned char)(i * 9);
    char t[512];
    int n = UpdateJobText(L"\\SDMemory\\VestaHomeInstall\\VestaHomeInstall.exe", "1.1.261130", sha, t, sizeof(t));
    Check(n > 0 && strstr(t, "version=1.1.261130\n") != NULL, "job: written");
    wchar_t exe[MAX_PATH];
    char v[32];
    Check(UpdateJobParse(t, n, exe, MAX_PATH, v, 32, back) &&
          !wcscmp(exe, L"\\SDMemory\\VestaHomeInstall\\VestaHomeInstall.exe") && !strcmp(v, "1.1.261130") &&
          !memcmp(sha, back, 32), "job: read back");
    Check(!UpdateJobParse("path=x\nversion=1\n", 18, exe, MAX_PATH, v, 32, back), "job: no SHA-256 - none");
    Check(!UpdateJobParse("path=x\nversion=1\nsha256=12\n", 27, exe, MAX_PATH, v, 32, back), "job: a short SHA - none");
    int need = UpdateJobText(L"x", "1", sha, t, sizeof(t));   // "path=x\nversion=1\nsha256=<64>\n"
    Check(need == 7 + 10 + 7 + 64 + 1 && t[need] == 0 && t[need - 1] == '\n', "job: its exact length, ended");
    Check(UpdateJobText(L"x", "1", sha, t, need + 1) == need, "job: just the room");
    Check(UpdateJobText(L"x", "1", sha, t, need) < 0, "job: a byte short - none");
    char bad[200];
    _snprintf(bad, 200, "path=x\nversion=1\nsha256=%064d\n", 0);
    bad[30] = 'z';   // not hex
    Check(!UpdateJobParse(bad, (int)strlen(bad), exe, MAX_PATH, v, 32, back), "job: not hex - none");
    n = UpdateJobText(L"\\SDMemory\\VestaHomeInstall\\VestaHomeInstall.exe", "1.1.261130", sha, t, sizeof(t));
    Check(!UpdateJobParse(t, n, exe, MAX_PATH, v, 10, back), "job: a version longer than its room - none");
    Check(!UpdateJobParse(t, n, exe, 5, v, 32, back), "job: a path longer than its room - none");
    Check(!UpdateJobParse(t + 1, n - 1, exe, MAX_PATH, v, 32, back), "job: no path line - none");
    char nov[300];
    int nn = _snprintf(nov, 300, "path=a\nsha256=%s", strstr(t, "sha256=") + 7);
    Check(!UpdateJobParse(nov, nn, exe, MAX_PATH, v, 32, back), "job: no version line - none");
}

int main()
{
    GetTempPathW(MAX_PATH, s_dir);
    TestMark();
    TestCompare();
    TestCheck();
    TestJob();
    printf("test_vupdate: %d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

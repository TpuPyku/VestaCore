// test_vhttp.cpp - vhttp.cpp against real servers from the PC (needs the internet; the TLS ones' roots must be in
// trust_anchors.inc - example.com's is not): plain and TLS answers, the
// headers in and out (If-None-Match / ETag, Range, Location), two threads at once, an endless stream (a radio).
// Exit code: the failed checks.
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vhttp.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void Quiet(const char*, ...) {}

static bool Get(const char* host, bool https, const char* path, int* status, unsigned char** body, int* len,
                const char* inm = 0, char* etag = 0, int etagCap = 0, const HttpExtra* extra = 0)
{
    *body = 0;
    *len = 0;
    return HttpGet(host, https, path, body, len, status, inm, etag, etagCap, extra) == NET_OK;
}

static const char* kRawHost = "raw.githubusercontent.com";   // ETag, If-None-Match and Range
static const char* kRawPath = "/TpuPyku/VestaCore/master/README.md";

static void TestPlainAndTls()
{
    int st, len;
    unsigned char* b;
    bool ok = Get("example.com", false, "/", &st, &b, &len);
    Check(ok && st == 200, "http example.com: 200");
    Check(b && strstr((char*)b, "Example Domain") != 0, "http: the page's body");
    free(b);
    ok = Get(kRawHost, true, kRawPath, &st, &b, &len);
    Check(ok && st == 200, "https raw.githubusercontent.com: 200");
    Check(b && !strncmp((char*)b, "# VestaCore", 11), "https: the file's body");
    free(b);
}

static void TestHeaders()
{
    int st, len;
    unsigned char* b;
    char etag[128];
    bool ok = Get(kRawHost, true, kRawPath, &st, &b, &len, 0, etag, sizeof(etag));
    free(b);
    Check(ok && etag[0] != 0, "the answer's ETag");
    char again[128];
    ok = Get(kRawHost, true, kRawPath, &st, &b, &len, etag, again, sizeof(again));
    free(b);
    Check(ok && st == 304, "If-None-Match: 304");

    HttpExtra range = { "Range: bytes=0-9\r\n", 0, 0 };
    ok = Get(kRawHost, true, kRawPath, &st, &b, &len, 0, 0, 0, &range);
    Check(ok && st == 206 && len == 10, "Range: 206, 10 bytes");
    free(b);

    char loc[256] = "";
    HttpExtra where = { 0, loc, sizeof(loc) };
    ok = Get("github.com", false, "/", &st, &b, &len, 0, 0, 0, &where);
    free(b);
    Check(ok && st == 301 && !strcmp(loc, "https://github.com/"), "Location of a redirect");
}

static DWORD WINAPI Worker(LPVOID p)
{
    const char* host = (const char*)p;
    int ok = 0;
    for (int i = 0; i < 3; i++) {
        int st, len;
        unsigned char* b;
        ok += Get(host, true, "/", &st, &b, &len) && st > 0;
        free(b);
    }
    return ok;
}

static void TestThreads()   // the pool shared by the threads: each its own connection, all answered
{
    const char* hosts[2] = { "raw.githubusercontent.com", "github.com" };
    HANDLE t[2];
    for (int i = 0; i < 2; i++)
        t[i] = CreateThread(0, 0, Worker, (LPVOID)hosts[i], 0, 0);
    WaitForMultipleObjects(2, t, TRUE, 60000);
    for (int i = 0; i < 2; i++) {
        DWORD n = 0;
        GetExitCodeThread(t[i], &n);
        CloseHandle(t[i]);
        Check(n == 3, i ? "thread 2: 3 answers" : "thread 1: 3 answers");
    }
}

static void TestStream()
{
    int err = -1, metaint = -1;
    char name[64], type[64];
    HttpStream* s = HttpStreamOpen("https://rusradio.hostingradio.ru/rusradio96.aacp", &err, &metaint, name,
                                   sizeof(name), type, sizeof(type));
    Check(s != 0, "a radio stream opened");
    if (!s) {
        printf("  (error %d)\n", err);
        return;
    }
    int got = 0;
    char buf[4096];
    for (int i = 0; i < 64 && got < 32768; i++) {
        int k = HttpStreamRead(s, buf, sizeof(buf));
        if (k <= 0)
            break;
        got += k;
    }
    Check(got >= 32768, "the stream's 32 KB");
    Check(strstr(type, "aac") != 0 || strstr(type, "audio") != 0, "the stream's Content-Type");
    HttpStreamAbort(s);
    Check(HttpStreamRead(s, buf, sizeof(buf)) <= 0, "an aborted stream reads nothing");
    HttpStreamClose(s);
}

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    static const HttpSetup setup = { "VestaCore-test/1.0", "*/*", 1024 * 1024, Quiet };
    HttpInit(&setup);
    TestPlainAndTls();
    TestHeaders();
    TestThreads();
    TestStream();
    HttpCloseAll();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

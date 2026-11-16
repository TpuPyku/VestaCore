// test_vhttp.cpp - vhttp.cpp against real servers from the PC (needs the internet; Yandex and Google - reachable
// where others are blocked, the user 2026-11-04; the TLS ones' roots must be in trust_anchors.inc): plain and TLS
// answers, the
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

static const char* kRawHost = "yastatic.net";   // a fixed file: ETag, If-None-Match and Range
static const char* kRawPath = "/jquery/3.3.1/jquery.min.js";

static void TestPlainAndTls()
{
    int st, len;
    unsigned char* b;
    bool ok = Get("www.google.com", false, "/robots.txt", &st, &b, &len);
    Check(ok && st == 200, "http www.google.com: 200");
    Check(b && !strncmp((char*)b, "User-agent:", 11), "http: the file's body");
    free(b);
    ok = Get(kRawHost, true, kRawPath, &st, &b, &len);
    Check(ok && st == 200, "https yastatic.net: 200");
    Check(b && !strncmp((char*)b, "/*! jQuery v3.3.1", 17), "https: the file's body");
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
    ok = Get("google.com", false, "/", &st, &b, &len, 0, 0, 0, &where);
    free(b);
    Check(ok && st == 301 && !strcmp(loc, "http://www.google.com/"), "Location of a redirect");
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
    const char* hosts[2] = { "yastatic.net", "www.google.com" };
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

static bool AnswerOf(NetStream* n, char* buf, int cap)   // HEAD / sent, the answer's start read
{
    static const char req[] = "HEAD / HTTP/1.0\r\nHost: x\r\n\r\n";
    if (NetWrite(n, req, sizeof(req) - 1) != 0)
        return false;
    int k = NetRead(n, buf, cap - 1, 8000);
    buf[k > 0 ? k : 0] = 0;
    return k > 0;
}

static void TestNet()   // a raw stream (MQTT): plain and TLS, waiting without breaking it, the caller's own root
{
    int err = -1;
    char buf[256];
    NetStream* n = NetOpen("www.google.com", 80, false, NULL, &err);
    Check(n && err == NET_OK, "net: plain TCP opened");
    if (n) {
        Check(NetRead(n, buf, sizeof(buf), 300) == 0, "net: nothing came - 0, not an error");
        Check(AnswerOf(n, buf, sizeof(buf)) && !strncmp(buf, "HTTP/1.", 7), "net: plain - written, read");
        NetAbort(n);
        Check(NetRead(n, buf, sizeof(buf), 300) < 0, "net: aborted - reads nothing");
        Check(NetWrite(n, "x", 1) < 0, "net: aborted - writes nothing");
        NetClose(n);
    }
    n = NetOpen("yastatic.net", 443, true, NULL, &err);
    Check(n && err == NET_OK, "net: TLS opened (the built-in roots)");
    if (n) {
        Check(NetRead(n, buf, sizeof(buf), 300) == 0, "net: TLS - nothing came yet, still whole");
        Check(AnswerOf(n, buf, sizeof(buf)) && !strncmp(buf, "HTTP/1.", 7), "net: TLS - written, read after waiting");
        NetClose(n);
    }
    static const unsigned char dn[] = { 0x30, 0x0B, 0x31, 0x09, 0x30, 0x07, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x00 };
    static unsigned char q[65] = { 0x04, 1, 2, 3 };   // (a root no server is signed by)
    NetCa own = { dn, sizeof(dn), q, sizeof(q) };
    n = NetOpen("yastatic.net", 443, true, &own, &err);
    Check(!n && err == NET_TLS, "net: TLS with the caller's own root only - a public server refused");
    hostent* he = gethostbyname("yastatic.net");   // a public server by its address: its name is still checked
    if (he) {
        in_addr a;
        memcpy(&a, he->h_addr_list[0], 4);
        n = NetOpen(inet_ntoa(a), 443, true, NULL, &err);
        Check(!n && err == NET_TLS, "net: the built-in roots by an IP address - the name still checked, refused");
        if (n)
            NetClose(n);
    }
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
    TestNet();
    HttpCloseAll();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

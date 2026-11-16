// vhttp.h - VestaCore: HTTP/1.1 client with keep-alive, plain or TLS (BearSSL 0.6: the program links its
// bearssl_*.lib and gives its include path), for VestaHome and OSMapsCE. The connections are shared by the program's
// threads; certificates are checked against trust_anchors.inc (make_ta.py) by the latest of the system clock, the GPS
// date and the build date (the unit's clock is reset on boot).
#ifndef VHTTP_H
#define VHTTP_H

enum { NET_OK = 0, NET_DNS, NET_CONNECT, NET_TIMEOUT, NET_HTTP, NET_DECODE, NET_MEMORY, NET_TLS };

struct HttpSetup {                       // the program's, given once to HttpInit
    const char* userAgent;               // e.g. "OSMapsCE/1.0 (+https://github.com/TpuPyku/OSMapsCE)"
    const char* accept;                  // the Accept header of HttpGet, e.g. "*/*"
    int         maxBody;                 // bytes at most in an answer's body
    void      (*log)(const char* fmt, ...);   // its log, NULL - none
};
void HttpInit(const HttpSetup* setup);   // before any other call

struct HttpExtra {             // a request's more: its headers, the answer's Location
    const char* headers;       // e.g. "Range: bytes=0-1048575\r\n" (each line with CR LF), NULL none
    char*       location;      // the answer's Location (a redirect), NULL not wanted
    int         locCap;
};
// The answer's body (malloc'ed, zero-terminated) in *body, the HTTP status in *status; NET_OK or an error. With
// ifNoneMatch the server may answer 304 (no body); the ETag of the answer goes to etag.
int  HttpGet(const char* host, bool https, const char* path, unsigned char** body, int* bodyLen, int* status,
             const char* ifNoneMatch = 0, char* etag = 0, int etagCap = 0, const HttpExtra* extra = 0);
void HttpSetGpsDate(int yyyymmdd);
void HttpCloseAll();

// An endless body (internet radio): http:// or https://, any port, redirects followed, ICY metadata asked for.
// *metaint = icy-metaint (0: none); name = icy-name, type = Content-Type (may be empty). NULL with *err.
struct HttpStream;
HttpStream* HttpStreamOpen(const char* url, int* err, int* metaint, char* name, int nameCap, char* type, int typeCap);
int  HttpStreamRead(HttpStream* s, char* buf, int len);   // the body's bytes; <= 0: closed, broken or aborted
void HttpStreamAbort(HttpStream* s);                       // from another thread: the blocked read returns
void HttpStreamClose(HttpStream* s);

// A raw byte stream (MQTT and the like): TCP, or TLS checked against the built-in roots or, with `ca`, against that
// root alone (the program's own CA: an EC P-256 key; a server named by its IP address is then not name-checked -
// BearSSL matches DNS names only, and the own CA signs only its own servers). NULL with *err.
struct NetCa {
    const unsigned char* dn;   // the root's subject, DER
    int dnLen;
    const unsigned char* q;    // its EC P-256 public point (65 bytes, uncompressed)
    int qLen;
};
struct NetStream;
NetStream* NetOpen(const char* host, int port, bool tls, const NetCa* ca, int* err);
int  NetRead(NetStream* n, char* buf, int len, int waitMs);   // bytes; 0: nothing in waitMs; < 0: closed or broken
int  NetWrite(NetStream* n, const void* data, int len);      // 0: all sent; < 0: broken
void NetAbort(NetStream* n);                                  // from another thread: a blocked read returns
void NetClose(NetStream* n);

#endif

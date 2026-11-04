// vjson.h - VestaCore: JSON read in place, no tree (OSMapsCE's routes, search and regions; VestaHome's radio
// catalogue). p points at a value; the text stays the caller's.
#ifndef VJSON_H
#define VJSON_H

#include <wchar.h>

const char* Ws(const char* p);                       // past the white space
const char* JSkip(const char* p);                    // past the value
const char* JKey(const char* p, const char* key);    // the object's value of key, NULL none
const char* JFirst(const char* p);                   // the array's first item, NULL none
const char* JNext(const char* p);                    // the next item, NULL none
double      JNum(const char* p, double def);
int         JStr(const char* p, char* out, int cap);   // a string, unescaped, UTF-8 (4 bytes of cap kept free)
void        JWide(const char* p, wchar_t* out, int cap);
void        UrlEncode(const char* s, char* out, int cap);   // all but letters, digits, -_.~ as %XX

#endif

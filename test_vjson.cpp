// test_vjson.cpp - vjson.cpp on the PC: keys of an object (past nested values), arrays, numbers, strings with
// escapes, URL encoding. Exit code: the failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vjson.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAILED: %s\n", what);
    }
}

static void TestKeys()
{
    const char* j = " { \"a\" : {\"x\":[1,{\"y\":\"}\"}]}, \"name\":\"Jazz \\\"24\\\"\", \"n\": -12.5 , \"e\":[] } ";
    char s[64];
    Check(JStr(JKey(j, "name"), s, sizeof(s)) == 9 && !strcmp(s, "Jazz \"24\""), "a key after a nested value");
    Check(JNum(JKey(j, "n"), 0) == -12.5, "a number");
    Check(JNum(JKey(j, "name"), 7) == 7, "not a number: the default");
    Check(!JKey(j, "nam"), "a key's beginning is not the key");
    Check(!JKey(j, "y"), "a nested key is not the object's");
    Check(!JKey("[1]", "a") && !JKey(NULL, "a"), "not an object: none");
    Check(!JFirst(JKey(j, "e")), "an empty array: no items");
    const char* x = JKey(j, "a");
    const char* arr = JKey(x, "x");
    const char* i1 = JFirst(arr);
    Check(i1 && JNum(i1, 0) == 1, "the first item");
    const char* i2 = JNext(i1);
    Check(i2 && JStr(JKey(i2, "y"), s, sizeof(s)) == 1 && !strcmp(s, "}"), "the next item, a brace in a string");
    Check(!JNext(i2), "no item after the last");
    Check(JNum(JKey("{\"a\":1,\"b\":2}", "b"), 0) == 2, "a key right after a comma");
    Check(JNum("0", 5) == 0 && JNum("9x", 5) == 9 && JNum("/", 5) == 5 && JNum(":", 5) == 5, "the digits' edges");
    Check(*Ws(" \t\r\n:") == ':', "past the white space");
    Check(*JSkip("\"a,b\" ,") == ' ' && *JSkip("{\"a\":[1,2]}x") == 'x' && *JSkip("123,") == ',', "past a value");
}

static void TestStrings()
{
    char s[16];
    JStr("\"\\u0410\\u044f\\/\\\\\"", s, sizeof(s));
    Check(!strcmp(s, "\xD0\x90\xD1\x8F/\\"), "\\u escapes as UTF-8, \\/ and \\\\");
    JStr("\"a\\nb\\tc\\rd\"", s, sizeof(s));
    Check(!strcmp(s, "a b cd"), "new lines and tabs as spaces, CR dropped");
    JStr("\"\\u00e9\"", s, sizeof(s));
    Check(!strcmp(s, "\xC3\xA9"), "a two-byte character");
    JStr("\"\\u0041\\u007f\\u0080\\u07ff\"", s, sizeof(s));
    Check(!strcmp(s, "A\x7F\xC2\x80\xDF\xBF"), "one byte up to 7F, two from 80 to 7FF");
    JStr("\"\\u0800\\u20ac\"", s, sizeof(s));
    Check(!strcmp(s, "\xE0\xA0\x80\xE2\x82\xAC"), "three bytes from 800 (the euro sign)");
    Check(JStr("\"ab\\", s, sizeof(s)) == 2 && !strcmp(s, "ab"), "a backslash at the very end");
    static const char cut[] = "\"ab\\\0zz\"";   // (a cut answer: something after the text's end)
    Check(JStr(cut, s, sizeof(s)) == 2 && !strcmp(s, "ab"), "nothing read past the text's end");
    Check(JStr("12", s, sizeof(s)) == 0 && !s[0], "not a string: empty");
    Check(JStr("\"abcdefghijklmnop\"", s, 8) == 4 && !strcmp(s, "abcd"), "cut to its room (4 bytes kept free)");
    wchar_t w[8];
    JWide("\"\\u0414\\u043e\\u043c\"", w, 8);
    Check(!wcscmp(w, L"\x0414\x043E\x043C"), "a string as UTF-16");
    wchar_t w4[4];
    JWide("\"abc\"", w4, 4);
    Check(!wcscmp(w4, L"abc"), "as UTF-16 into exactly its room");
    JWide("\"abcdef\"", w4, 4);
    Check(!w4[0], "too long for its room: empty");
}

static void TestUrl()
{
    char s[64];
    UrlEncode("jazz & blues/ä-_.~", s, sizeof(s));
    Check(!strcmp(s, "jazz%20%26%20blues%2F%C3%A4-_.~"), "URL encoding");
    UrlEncode("azAZ09`{@[/:", s, sizeof(s));
    Check(!strcmp(s, "azAZ09%60%7B%40%5B%2F%3A"), "the letters' and digits' edges");
    UrlEncode("abcdefgh", s, 6);
    Check(!strcmp(s, "ab"), "cut to its room");
}

int main()
{
    TestKeys();
    TestStrings();
    TestUrl();
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

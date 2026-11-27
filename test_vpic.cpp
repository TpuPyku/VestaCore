// test_vpic.cpp - vpic.cpp on the PC (test_vpic.bat): pictures made here (BMP), VestaHome's test photos when there
// (TJpgDec's baseline JPEG, stb's progressive one).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vpic.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

static int s_failed, s_checks;

static void Check(bool ok, const char* what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL: %s\n", what);
    }
}

// A w x h 24-bit BMP: the left half red, the right half blue.
static void MakeBmp(const wchar_t* path, int w, int h)
{
    int row = (w * 3 + 3) & ~3, size = 54 + row * h;
    unsigned char* b = (unsigned char*)calloc(size, 1);
    b[0] = 'B';
    b[1] = 'M';
    *(int*)(b + 2) = size;
    *(int*)(b + 10) = 54;
    *(int*)(b + 14) = 40;
    *(int*)(b + 18) = w;
    *(int*)(b + 22) = h;
    *(short*)(b + 26) = 1;
    *(short*)(b + 28) = 24;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned char* p = b + 54 + y * row + x * 3;   // B G R
            p[x < w / 2 ? 2 : 0] = 255;
        }
    FILE* f = _wfopen(path, L"wb");
    fwrite(b, 1, size, f);
    fclose(f);
    free(b);
}

static void TestNames()
{
    Check(PicIsName(L"a.jpg") && PicIsName(L"b.JPEG") && PicIsName(L"c.png") && PicIsName(L"d.bmp"), "name: pictures");
    Check(!PicIsName(L"a.gif") && !PicIsName(L"jpg") && !PicIsName(L"a.jpg.txt"), "name: not pictures");
}

static void TestBmp(const wchar_t* dir)
{
    wchar_t bmp[MAX_PATH], junk[MAX_PATH], none[MAX_PATH];
    _snwprintf(bmp, MAX_PATH, L"%s\\wide.bmp", dir);
    _snwprintf(junk, MAX_PATH, L"%s\\junk.jpg", dir);
    _snwprintf(none, MAX_PATH, L"%s\\none.jpg", dir);
    MakeBmp(bmp, 200, 100);
    FILE* f = _wfopen(junk, L"wb");
    fputs("not a picture at all, only text", f);
    fclose(f);

    Pic p;
    int sw = 0, sh = 0;
    Check(PicLoad(bmp, 100, 100, false, 0, false, &p, &sw, &sh, false) == PIC_OK && p.w == 100 && p.h == 50 && p.px &&
          sw == 200 && sh == 100, "bmp fit: the picture's proportions inside the box, its own size told");
    if (p.px) {
        unsigned short l = p.px[25 * 100 + 5], r = p.px[25 * 100 + 95];
        Check(l >> 11 == 31 && (l & 31) == 0 && r >> 11 == 0 && (r & 31) == 31, "bmp fit: RGB565, red left, blue right");
        PicFree(&p);
    }
    Check(PicLoad(bmp, 60, 60, true, 0, false, &p, NULL, NULL, true) == PIC_OK && p.w == 60 && p.h == 60 && p.px32,
          "bmp fill: the box covered, 32-bit");
    PicFree(&p);
    Check(PicLoad(bmp, 100, 100, false, 1, false, &p, NULL, NULL, false) == PIC_OK && p.w == 50 && p.h == 100,
          "bmp turned a quarter: taller than wide");
    PicFree(&p);
    Check(PicThumb(bmp, 112, 60, false, &p) == PIC_OK && p.w == 112 && p.h == 60 && p.px, "thumb: the tile's box");
    PicFree(&p);
    Check(PicThumb(junk, 112, 60, false, &p) != PIC_OK && !p.px, "thumb: not a picture - an error, nothing kept");
    Check(PicThumb(none, 112, 60, false, &p) == PIC_READ, "thumb: no file - a read error");
    // a big file named .jpg that is no picture (zeros: a broken copy): refused at once, not after seconds (the
    // previews' one worker stood 4.7 s on the PC on one - VestaFiles 2026-11-27)
    FILE* z = _wfopen(junk, L"wb");
    char zeros[4096] = { 0 };
    for (int i = 0; i < 600; i++)
        fwrite(zeros, 1, sizeof(zeros), z);
    fclose(z);
    DWORD t0 = GetTickCount();
    Check(PicThumb(junk, 112, 60, true, &p) != PIC_OK && GetTickCount() - t0 < 300, "thumb: 2.4 MB of zeros - refused fast");
    DeleteFile(bmp);
    DeleteFile(junk);
}

static void TestJpeg(const wchar_t* photos)   // VestaHome's test photos (make_testmedia.py): there or skipped
{
    wchar_t base[MAX_PATH], prog[MAX_PATH];
    _snwprintf(base, MAX_PATH, L"%s\\IMG_0002.jpg", photos);
    _snwprintf(prog, MAX_PATH, L"%s\\IMG_0003_progressive.jpg", photos);
    if (GetFileAttributes(base) == 0xFFFFFFFF || GetFileAttributes(prog) == 0xFFFFFFFF) {
        printf("(no test photos in %ls - JPEG checks skipped)\n", photos);
        return;
    }
    Pic p;
    Check(PicThumb(base, 112, 60, true, &p) == PIC_OK && p.w == 112 && p.h == 60, "jpeg thumb: baseline (TJpgDec)");
    PicFree(&p);
    Check(PicThumb(prog, 112, 60, true, &p) == PIC_OK && p.w == 112 && p.h == 60, "jpeg thumb: progressive (stb)");
    PicFree(&p);
}

int wmain(int argc, wchar_t** argv)
{
    wchar_t dir[MAX_PATH];
    GetTempPath(MAX_PATH, dir);
    wcscat(dir, L"test_vpic");
    CreateDirectory(dir, NULL);
    TestNames();
    TestBmp(dir);
    TestJpeg(argc > 1 ? argv[1] : L".");
    RemoveDirectory(dir);
    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed;
}

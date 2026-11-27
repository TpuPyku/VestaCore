// vpic.cpp - VestaCore: pictures (VestaFiles' previews; VestaHome's photo.cpp moves here). JPEG through TJpgDec
// (the program's third_party/tjpgd): decoded in full colour and scaled down by 1/2..1/8 while decoding, with a few KB of working memory, the file read as a stream - a
// 12 MP photo never needs its full size in RAM. PNG, BMP and the JPEGs TJpgDec can't read (progressive) through
// stb_image, up to kStbMaxPx. The EXIF thumbnail and orientation. The result 32-bit (0x00RRGGBB, the viewer: the
// user 2026-10-23) or RGB565 (the folders' thumbnails: memory), rows top-down. No UI, no threads: the caller
// decides where it runs (a photo takes a second or two on the unit).
#pragma once
#include <windows.h>

struct Pic {
    int             w, h;
    unsigned short* px;     // w * h RGB565, malloc'ed (PicFree) - or
    DWORD*          px32;   // w * h 0x00RRGGBB (one of the two)
};

enum { PIC_OK, PIC_FORMAT, PIC_BIG, PIC_READ, PIC_MEMORY };

// Whether the file name is a picture these read: .jpg .jpeg .png .bmp
bool PicIsName(const wchar_t* name);

// The picture fitted into box w x h (fill: covering the box, the overflow cut off; otherwise all of it inside),
// turned rot quarter turns clockwise, plus the EXIF orientation when exifRot. The result's size: fill - the box;
// fit - the picture's own proportions, at most the box. srcW / srcH (may be NULL): the file's own size. deep: the
// result 32-bit, otherwise RGB565.
int  PicLoad(const wchar_t* path, int boxW, int boxH, bool fill, int rot, bool exifRot, Pic* out, int* srcW,
             int* srcH, bool deep);

// The same from a picture in memory (a JPEG or a PNG inside an mp3's tag), no turning.
int  PicLoadMem(const unsigned char* data, int len, int boxW, int boxH, bool fill, Pic* out, bool deep);

// A 32-bit picture fitted (fill: covering) into box w x h: a smaller copy, 32-bit (deep) or RGB565.
int  PicFit(const Pic* src, int boxW, int boxH, bool fill, Pic* out, bool deep);

// A thumbnail filling w x h (RGB565): the EXIF thumbnail when the JPEG has one (fast), else the picture at 1/8.
int  PicThumb(const wchar_t* path, int w, int h, bool exifRot, Pic* out);

void PicFree(Pic* p);

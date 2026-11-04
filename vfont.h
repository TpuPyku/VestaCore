// vfont.h - VestaCore: the text's fonts of VestaHome and OSMapsCE.
#ifndef VFONT_H
#define VFONT_H

#include <windows.h>

// A font of `face` with characters `height` px tall, not smoothed: on the unit ANTIALIASED_QUALITY comes out as
// ClearType - coloured fringes in the 16-bit frame, small text blurred (the user 2026-10-17); the faces are hinted
// for crisp unsmoothed pixels. The caller keeps it and deletes it.
HFONT VFontMake(const wchar_t* face, int height, int weight);

#endif

// vfont.cpp - VestaCore: the text's fonts. See vfont.h.
#include "vfont.h"
#include <string.h>

HFONT VFontMake(const wchar_t* face, int height, int weight)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = NONANTIALIASED_QUALITY;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    return CreateFontIndirect(&lf);
}

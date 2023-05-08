/*
 Copyright (C) 2023 jbleyel

 displayvfd is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 dogtag is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with displayvfd.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _UPNG_H_
#define _UPNG_H_

#define PNG_SKIP_SETJMP_CHECK
#include <png.h>
#include <cstdint>
#include <string>
#include "erect.h"

struct gRGB
{
    union {
#if BYTE_ORDER == LITTLE_ENDIAN
        struct {
            unsigned char b, g, r, a;
        };
#else
        struct {
            unsigned char a, r, g, b;
        };
#endif
        uint32_t value;
    };
    gRGB(int r, int g, int b, int a=0): b(b), g(g), r(r), a(a)
    {
    }
    gRGB(uint32_t val): value(val)
    {
    }
    gRGB(const gRGB& other): value(other.value)
    {
    }
    gRGB(const char *colorstring)
    {
        uint32_t val = 0;

        if (colorstring)
        {
            for (int i = 0; i < 8; i++)
            {
                char c = colorstring[i];
                if (!c) break;
                val <<= 4;
                if (c >= '0' && c <= '9')
                    val |= c - '0';
                else if(c >= 'a' && c <= 'f')
                    val |= c - 'a' + 10;
                else if(c >= 'A' && c <= 'F')
                    val |= c - 'A' + 10;
                else if(c >= ':' && c <= '?') // Backwards compatibility for old style color strings
                    val |= c & 0x0f;
            }
        }
        value = val;
    }
    gRGB(): value(0)
    {
    }

    uint32_t argb() const
    {
        return value;
    }

    void set(uint32_t val)
    {
        value = val;
    }

    void operator=(uint32_t val)
    {
        value = val;
    }
    bool operator < (const gRGB &c) const
    {
        if (b < c.b)
            return true;
        if (b == c.b)
        {
            if (g < c.g)
                return true;
            if (g == c.g)
            {
                if (r < c.r)
                    return true;
                if (r == c.r)
                    return a < c.a;
            }
        }
        return false;
    }
    bool operator==(const gRGB &c) const
    {
        return c.value == value;
    }
    bool operator != (const gRGB &c) const
    {
        return c.value != value;
    }
    operator const std::string () const
    {
        uint32_t val = value;
        std::string escapecolor = "\\c";
        escapecolor.resize(10);
        for (int i = 9; i >= 2; i--)
        {
            int hexbits = val & 0xf;
            escapecolor[i] = hexbits < 10    ? '0' + hexbits
                            : 'a' - 10 + hexbits;
            val >>= 4;
        }
        return escapecolor;
    }
    void alpha_blend(const gRGB other)
    {
#define BLEND(x, y, a) (y + (((x-y) * a)>>8))
        b = BLEND(other.b, b, other.a);
        g = BLEND(other.g, g, other.a);
        r = BLEND(other.r, r, other.a);
        a = BLEND(0xFF, a, other.a);
#undef BLEND
    }
};


struct gPalette
{
    int start, colors;
    gRGB *data;
};


struct gUnmanagedSurface
{
    int x, y, bpp, bypp, stride;
    gPalette clut;
    void *data;
    int data_phys;

    gUnmanagedSurface();
    gUnmanagedSurface(int width, int height, int bpp);
};

struct gSurface: gUnmanagedSurface
{
    gSurface(): gUnmanagedSurface() {}
    gSurface(int width, int height, int bpp);
    ~gSurface();
private:
    gSurface(const gSurface&); /* Copying managed gSurface is not allowed */
    gSurface& operator =(const gSurface&);
};

class uPNG
{
public:
    
    enum
    {
        blitAlphaTest=1,
        blitAlphaBlend=2,
        blitScale=4,
        blitKeepAspectRatio=8,
        blitHAlignCenter = 16,
        blitHAlignRight = 32,
        blitVAlignCenter = 64,
        blitVAlignBottom = 128
    };

	uPNG();
	~uPNG();
    gUnmanagedSurface* loadPNG(const char* filename);
//	void blit(unsigned char* dest, int posX, int posY, int width, int height, int bpp, int flag);
	int render(const char* filename, int posX, int posY, gUnmanagedSurface* dest, int width, int height, int bpp, int flag = blitAlphaTest);
	
    int blit(gUnmanagedSurface * surface, const int src_w, const int src_h, const eRect &_pos, /*const gRegion &clip, */int flag);
    
private:
    gUnmanagedSurface *m_surface;
};

#endif

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

#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include "erect.h"
#include "upng.h"

#ifndef __DARWIN_LITTLE_ENDIAN
#include <byteswap.h>
#else

#define bswap_16(value) \
((((value) & 0xff) << 8) | ((value) >> 8))

#endif


gUnmanagedSurface::gUnmanagedSurface():
    x(0), y(0), bpp(0), bypp(0), stride(0),
    data(0),
    data_phys(0)
{
}

gUnmanagedSurface::gUnmanagedSurface(int width, int height, int _bpp):
    x(width),
    y(height),
    bpp(_bpp),
    data(0),
    data_phys(0)
{
    switch (_bpp)
    {
    case 8:
        bypp = 1;
        break;
    case 15:
    case 16:
        bypp = 2;
        break;
    case 24:        // never use 24bit mode
    case 32:
        bypp = 4;
        break;
    default:
        bypp = (bpp+7)/8;
    }
    stride = x*bypp;
}


gSurface::gSurface(int width, int height, int _bpp):
    gUnmanagedSurface(width, height, _bpp)
{
    if (!data)
    {
        data = new unsigned char [y * stride];
    }
}

gSurface::~gSurface()
{
    if (data)
    {
        delete [] (unsigned char*)data;
    }
    if (clut.data)
    {
        delete [] clut.data;
    }
}

uPNG::uPNG()
{
	m_surface = NULL;
}
uPNG::~uPNG()
{
	if(m_surface != NULL)
		delete m_surface;
}

gUnmanagedSurface* uPNG::loadPNG(const char* filename)
{
    FILE *fp=fopen(filename, "rb");
    unsigned char header[8];

    
    if (!fp)
    {
        printf("[uPNG] couldn't open %s\n", filename );
        return NULL;
    }
    if (!fread(header, 8, 1, fp))
    {
        printf("[uPNG] couldn't read\n");
        fclose(fp);
        return NULL;
    }
    if (png_sig_cmp(header, 0, 8))
    {
        fclose(fp);
        return NULL;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (!png_ptr)
    {
        printf("[uPNG] failed to create read struct\n");
        fclose(fp);
        return NULL;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        printf("[uPNG] failed to create info struct\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        fclose(fp);
        return NULL;
    }
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        printf("[uPNG] failed to create end info struct\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        fclose(fp);
        return NULL;
    }
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("[uPNG] png setjump failed or activated\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return NULL;
    }
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width, height;
    int bit_depth;
    int color_type;
    int interlace_type;
    int channels;
    int trns;

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, 0, 0);
    channels = png_get_channels(png_ptr, info_ptr);
    trns = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS);
    //eDebug("[ePNG] %s: before %dx%dx%dbpcx%dchan coltyp=%d", filename, (int)width, (int)height, bit_depth, channels, color_type);

    /*
     * gPixmaps use 8 bits per channel. rgb pixmaps are stored as abgr.
     * So convert 1,2 and 4 bpc to 8bpc images that enigma can blit
     * so add 'empty' alpha channel
     * Expand G+tRNS to GA, RGB+tRNS to RGBA
     */
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (bit_depth < 8)
        png_set_packing (png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && trns)
        png_set_tRNS_to_alpha(png_ptr);
    if ((color_type == PNG_COLOR_TYPE_GRAY && trns) || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
        png_set_bgr(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_RGB) {
        if (trns)
            png_set_tRNS_to_alpha(png_ptr);
        else
            png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);
    }

    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        png_set_bgr(png_ptr);

    // Update the info structures after the transformations take effect
    if (interlace_type != PNG_INTERLACE_NONE)
        png_set_interlace_handling(png_ptr);  // needed before read_update_info()
    png_read_update_info (png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, 0, 0, 0);
    channels = png_get_channels(png_ptr, info_ptr);

  //  result = new gPixmap(width, height, bit_depth * channels, cached ? PixmapCache::PixmapDisposed : NULL, accel);
    gUnmanagedSurface *surface = new gSurface(width, height, bit_depth * channels);
    
    png_bytep *rowptr = new png_bytep[height];
    for (unsigned int i = 0; i < height; i++)
        rowptr[i] = ((png_byte*)(surface->data)) + i * surface->stride;
    png_read_image(png_ptr, rowptr);

    delete [] rowptr;

    int num_palette = -1, num_trans = -1;
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)) {
            png_color *palette;
            png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
            if (num_palette) {
                surface->clut.data = new gRGB[num_palette];
                surface->clut.colors = num_palette;

                for (int i = 0; i < num_palette; i++) {
                    surface->clut.data[i].a = 0;
                    surface->clut.data[i].r = palette[i].red;
                    surface->clut.data[i].g = palette[i].green;
                    surface->clut.data[i].b = palette[i].blue;
                }

                if (trns) {
                    png_byte *trans;
                    png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, 0);
                    for (int i = 0; i < num_trans; i++)
                        surface->clut.data[i].a = 255 - trans[i];
                    for (int i = num_trans; i < num_palette; i++)
                        surface->clut.data[i].a = 0;
                }

            }
            else {
                surface->clut.data = 0;
                surface->clut.colors = num_palette;
            }
        }
        else {
            surface->clut.data = 0;
            surface->clut.colors = 0;
        }
        surface->clut.start = 0;
    }


//    printf("[uPNG] %s: after  %dx%dx%dbpcx%dchan coltyp=%d cols=%d trans=%d\n", filename, (int)width, (int)height, bit_depth, channels, color_type, num_palette, num_trans);

    png_read_end(png_ptr, end_info);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fp);
    return surface;
}

static inline void blit_8i_to_32(uint32_t *dst, const uint8_t *src, const uint32_t *pal, int width)
{
    while (width--)
        *dst++=pal[*src++];
}

static inline void blit_8i_to_32_at(uint32_t *dst, const uint8_t *src, const uint32_t *pal, int width)
{
    while (width--)
    {
        if (!(pal[*src]&0x80000000))
        {
            src++;
            dst++;
        } else
            *dst++=pal[*src++];
    }
}

static inline void blit_8i_to_16(uint16_t *dst, const uint8_t *src, const uint32_t *pal, int width)
{
    while (width--)
        *dst++=pal[*src++] & 0xFFFF;
}

static inline void blit_8i_to_16_at(uint16_t *dst, const uint8_t *src, const uint32_t *pal, int width)
{
    while (width--)
    {
        if (!(pal[*src]&0x80000000))
        {
            src++;
            dst++;
        } else
            *dst++=pal[*src++] & 0xFFFF;
    }
}

static void blit_8i_to_32_ab(gRGB *dst, const uint8_t *src, const gRGB *pal, int width)
{
    while (width--)
    {
        dst->alpha_blend(pal[*src++]);
        ++dst;
    }
}

static void convert_palette(uint32_t* pal, const gPalette& clut)
{
    int i = 0;
    if (clut.data)
    {
        while (i < clut.colors)
        {
            pal[i] = clut.data[i].argb() ^ 0xFF000000;
            ++i;
        }
    }
    for(; i != 256; ++i)
    {
        pal[i] = (0x010101*i) | 0xFF000000;
    }
}

#define FIX 0x10000


int uPNG::blit(gUnmanagedSurface * surface, const int src_w, const int src_h, const eRect &_pos, /*const gRegion &clip, */int flag)
{
    eRect pos = _pos;
    eSize src_size = eSize(src_w,src_h);

//    eDebug("[gPixmap] source size: %d %d", src.size().width(), src.size().height());

    int scale_x = FIX, scale_y = FIX;

    if (!(flag & blitScale))
    {
        // pos' size is ignored if left or top aligning.
        // if its size isn't set, centre and right/bottom aligning is ignored
        
        if (_pos.size().isValid())
        {
            if (flag & blitHAlignCenter)
                pos.setLeft(_pos.left() + (_pos.width() - src_w) / 2);
            else if (flag & blitHAlignRight)
                pos.setLeft(_pos.right() - src_w);

            if (flag & blitVAlignCenter)
                pos.setTop(_pos.top() + (_pos.height() - src_h) / 2);
            else if (flag & blitVAlignBottom)
                pos.setTop(_pos.bottom() - src_h);
        }

        pos.setWidth(src_w);
        pos.setHeight(src_h);
    }
    else if (pos.size() == src_size) /* no scaling required */
        flag &= ~blitScale;
    else // blitScale is set
    {
        scale_x = pos.size().width() * FIX / src_w; //NOSONAR
        scale_y = pos.size().height() * FIX / src_h; //NOSONAR
        if (flag & blitKeepAspectRatio)
        {
            if (scale_x > scale_y)
            {
                // vertical is full height, adjust horizontal to be smaller
                scale_x = scale_y;
                pos.setWidth(src_w * _pos.height() / src_h);
                if (flag & blitHAlignCenter)
                    pos.moveBy((_pos.width() - pos.width()) / 2, 0);
                else if (flag & blitHAlignRight)
                    pos.moveBy(_pos.width() - pos.width(), 0);
            }
            else
            {
                // horizontal is full width, adjust vertical to be smaller
                scale_y = scale_x;
                pos.setHeight(src_h * _pos.width() / src_w);
                if (flag & blitVAlignCenter)
                    pos.moveBy(0, (_pos.height() - pos.height()) / 2);
                else if (flag & blitVAlignBottom)
                    pos.moveBy(0, _pos.height() - pos.height());
            }
        }
    }


//    eDebug("[gPixmap] SCALE %x %x", scale_x, scale_y);

    for (unsigned int cci=0; cci<1; ++cci)
//    for (unsigned int i=0; i<clip.rects.size(); ++i)
    {
//        eDebug("[gPixmap] clip rect: %d %d %d %d", clip.rects[i].x(), clip.rects[i].y(), clip.rects[i].width(), clip.rects[i].height());
        eRect area = pos; /* pos is the virtual (pre-clipping) area on the dest, which can be larger/smaller than src if scaling is enabled */
      
        //  area&=clip.rects[i];
        
        area&=eRect(ePoint(0, 0), src_size);

        if (area.empty())
            continue;

        eRect srcarea = area;
        srcarea.moveBy(-pos.x(), -pos.y());

//        eDebug("[gPixmap] srcarea before scale: %d %d %d %d",
//            srcarea.x(), srcarea.y(), srcarea.width(), srcarea.height());

        if (flag & blitScale)
            srcarea = eRect(srcarea.x() * FIX / scale_x, srcarea.y() * FIX / scale_y, srcarea.width() * FIX / scale_x, srcarea.height() * FIX / scale_y);

//        eDebug("[gPixmap] srcarea after scale: %d %d %d %d",
//            srcarea.x(), srcarea.y(), srcarea.width(), srcarea.height());


        if (flag & blitScale)
        {
            if ((surface->bpp == 32) && (m_surface->bpp==8))
            {
                const uint8_t *srcptr = (uint8_t*)m_surface->data;
                uint8_t *dstptr=(uint8_t*)surface->data; // !!
                uint32_t pal[256];
                convert_palette(pal, m_surface->clut);

                const int src_stride = m_surface->stride;
                srcptr += srcarea.left()*m_surface->bypp + srcarea.top()*src_stride;
                dstptr += area.left()*surface->bypp + area.top()*surface->stride;
                const int width = area.width();
                const int height = area.height();
                const int src_height = srcarea.height();
                const int src_width = srcarea.width();
                if (flag & blitAlphaTest)
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const uint8_t *src_row_ptr = srcptr + (((y * src_height) / height) * src_stride);
                        uint32_t *dst = (uint32_t*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            uint32_t pixel = pal[src_row_ptr[(x *src_width) / width]];
                            if (pixel & 0x80000000)
                                *dst = pixel;
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
                else if (flag & blitAlphaBlend)
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const uint8_t *src_row_ptr = srcptr + (((y * src_height) / height) * src_stride);
                        gRGB *dst = (gRGB*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            dst->alpha_blend(pal[src_row_ptr[(x * src_width) / width]]);
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
                else
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const uint8_t *src_row_ptr = srcptr + (((y * src_height) / height) * src_stride);
                        uint32_t *dst = (uint32_t*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            *dst = pal[src_row_ptr[(x * src_width) / width]];
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
            }
            else if ((surface->bpp == 32) && (m_surface->bpp == 32))
            {
                const int src_stride = m_surface->stride;
                const uint8_t* srcptr = (const uint8_t*)m_surface->data + srcarea.left()*m_surface->bypp + srcarea.top()*src_stride;
                uint8_t* dstptr = (uint8_t*)surface->data + area.left()*surface->bypp + area.top()*surface->stride;
                const int width = area.width();
                const int height = area.height();
                const int src_height = srcarea.height();
                const int src_width = srcarea.width();
                if (flag & blitAlphaTest)
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const uint32_t *src_row_ptr = (uint32_t*)(srcptr + (((y * src_height) / height) * src_stride));
                        uint32_t *dst = (uint32_t*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            uint32_t pixel = src_row_ptr[(x *src_width) / width];
                            if (pixel & 0x80000000)
                                *dst = pixel;
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
                else if (flag & blitAlphaBlend)
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const gRGB *src_row_ptr = (gRGB *)(srcptr + (((y * src_height) / height) * src_stride));
                        gRGB *dst = (gRGB*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            dst->alpha_blend(src_row_ptr[(x * src_width) / width]);
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
                else
                {
                    for (int y = 0; y < height; ++y)
                    {
                        const uint32_t *src_row_ptr = (uint32_t*)(srcptr + (((y * src_height) / height) * src_stride));
                        uint32_t *dst = (uint32_t*)dstptr;
                        for (int x = 0; x < width; ++x)
                        {
                            *dst = src_row_ptr[(x * src_width) / width];
                            ++dst;
                        }
                        dstptr += surface->stride;
                    }
                }
            }
            else
            {
                printf("[uPNG] unimplemented: scale on non-accel surface %d->%d bpp\n", m_surface->bpp, surface->bpp);
            }
            continue;
        }

        if ((surface->bpp == 8) && (m_surface->bpp == 8))
        {
            uint8_t *srcptr=(uint8_t*)m_surface->data;
            uint8_t *dstptr=(uint8_t*)surface->data;

            srcptr+=srcarea.left()*m_surface->bypp+srcarea.top()*m_surface->stride;
            dstptr+=area.left()*surface->bypp+area.top()*surface->stride;
            if (flag & (blitAlphaTest|blitAlphaBlend))
            {
                for (int y = area.height(); y != 0; --y)
                {
                    // no real alphatest yet
                    int width=area.width();
                    unsigned char *s = (unsigned char*)srcptr;
                    unsigned char *d = (unsigned char*)dstptr;
                    // use duff's device here!
                    while (width--)
                    {
                        if (!*s)
                        {
                            s++;
                            d++;
                        }
                        else
                        {
                            *d++ = *s++;
                        }
                    }
                    srcptr += m_surface->stride;
                    dstptr += surface->stride;
                }
            }
            else
            {
                int linesize = area.width()*surface->bypp;
                for (int y = area.height(); y != 0; --y)
                {
                    memcpy(dstptr, srcptr, linesize);
                    srcptr += m_surface->stride;
                    dstptr += surface->stride;
                }
            }
        }
        else if ((surface->bpp == 32) && (m_surface->bpp==32))
        {
            uint32_t *srcptr=(uint32_t*)m_surface->data;
            uint32_t *dstptr=(uint32_t*)surface->data;

            srcptr+=srcarea.left()+srcarea.top()*m_surface->stride/4;
            dstptr+=area.left()+area.top()*surface->stride/4;
            for (int y = area.height(); y != 0; --y)
            {
                if (flag & blitAlphaTest)
                {
                    int width = area.width();
                    uint32_t *src = srcptr;
                    uint32_t *dst = dstptr;

                    while (width--)
                    {
                        if (!((*src)&0xFF000000))
                        {
                            src++;
                            dst++;
                        } else
                            *dst++=*src++;
                    }
                } else if (flag & blitAlphaBlend)
                {
                    int width = area.width();
                    gRGB *src = (gRGB*)srcptr;
                    gRGB *dst = (gRGB*)dstptr;
                    while (width--)
                    {
                        dst->alpha_blend(*src++);
                        ++dst;
                    }
                }
                else
                    memcpy(dstptr, srcptr, area.width()*surface->bypp);
                srcptr = (uint32_t*)((uint8_t*)srcptr + m_surface->stride);
                dstptr = (uint32_t*)((uint8_t*)dstptr + surface->stride);
            }
        }
        else if ((surface->bpp == 32) && (m_surface->bpp==8))
        {
            const uint8_t *srcptr = (uint8_t*)m_surface->data;
            uint8_t *dstptr=(uint8_t*)surface->data; // !!
            uint32_t pal[256];
            convert_palette(pal, m_surface->clut);

            srcptr+=srcarea.left()*m_surface->bypp+srcarea.top()*m_surface->stride;
            dstptr+=area.left()*surface->bypp+area.top()*surface->stride;
            const int width=area.width();
            for (int y = area.height(); y != 0; --y)
            {
                if (flag & blitAlphaTest)
                    blit_8i_to_32_at((uint32_t*)dstptr, srcptr, pal, width);
                else if (flag & blitAlphaBlend)
                    blit_8i_to_32_ab((gRGB*)dstptr, srcptr, (const gRGB*)pal, width);
                else
                    blit_8i_to_32((uint32_t*)dstptr, srcptr, pal, width);
                srcptr += m_surface->stride;
                dstptr += surface->stride;
            }
        }
        else if ((surface->bpp == 16) && (m_surface->bpp==8))
        {
            uint8_t *srcptr=(uint8_t*)m_surface->data;
            uint8_t *dstptr=(uint8_t*)surface->data; // !!
            uint32_t pal[256];

            for (int i=0; i != 256; ++i)
            {
                uint32_t icol;
                if (m_surface->clut.data && (i<m_surface->clut.colors))
                    icol = m_surface->clut.data[i].argb();
                else
                    icol=0x010101*i;
#if BYTE_ORDER == LITTLE_ENDIAN
                pal[i] = bswap_16(((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19);
#else
                pal[i] = ((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19;
#endif
                pal[i]^=0xFF000000;
            }

            srcptr+=srcarea.left()*m_surface->bypp+srcarea.top()*m_surface->stride;
            dstptr+=area.left()*surface->bypp+area.top()*surface->stride;

            if (flag & blitAlphaBlend)
                printf("[uPNG] ignore unsupported 8bpp -> 16bpp alphablend!\n");

            for (int y=0; y<area.height(); y++)
            {
                int width=area.width();
                unsigned char *psrc=(unsigned char*)srcptr;
                uint16_t *dst=(uint16_t*)dstptr;
                if (flag & blitAlphaTest)
                    blit_8i_to_16_at(dst, psrc, pal, width);
                else
                    blit_8i_to_16(dst, psrc, pal, width);
                srcptr+=m_surface->stride;
                dstptr+=surface->stride;
            }
        }
        else if ((surface->bpp == 16) && (m_surface->bpp==32))
        {
            uint8_t *srcptr=(uint8_t*)m_surface->data;
            uint8_t *dstptr=(uint8_t*)surface->data;

            srcptr+=srcarea.left()*m_surface->bypp+srcarea.top()*m_surface->stride;
            dstptr+=area.left()*surface->bypp+area.top()*surface->stride;

            for (int y=0; y<area.height(); y++)
            {
                int width=area.width();
                uint32_t *srcp=(uint32_t*)srcptr;
                uint16_t *dstp=(uint16_t*)dstptr;

                if (flag & blitAlphaBlend)
                {
                    while (width--)
                    {
                        if (!((*srcp)&0xFF000000))
                        {
                            srcp++;
                            dstp++;
                        } else
                        {
                            gRGB icol = *srcp++;
#if BYTE_ORDER == LITTLE_ENDIAN
                            uint32_t jcol = bswap_16(*dstp);
#else
                            uint32_t jcol = *dstp;
#endif
                            int bg_b = (jcol >> 8) & 0xF8;
                            int bg_g = (jcol >> 3) & 0xFC;
                            int bg_r = (jcol << 3) & 0xF8;

                            int a = icol.a;
                            int r = icol.r;
                            int g = icol.g;
                            int b = icol.b;

                            r = ((r-bg_r)*a)/255 + bg_r;
                            g = ((g-bg_g)*a)/255 + bg_g;
                            b = ((b-bg_b)*a)/255 + bg_b;

#if BYTE_ORDER == LITTLE_ENDIAN
                            *dstp++ = bswap_16( (b >> 3) << 11 | (g >> 2) << 5 | r  >> 3 );
#else
                            *dstp++ = (b >> 3) << 11 | (g >> 2) << 5 | r  >> 3 ;
#endif
                        }
                    }
                }
                else if (flag & blitAlphaTest)
                {
                    while (width--)
                    {
                        if (!((*srcp)&0xFF000000))
                        {
                            srcp++;
                            dstp++;
                        } else
                        {
                            uint32_t icol = *srcp++;
#if BYTE_ORDER == LITTLE_ENDIAN
                            *dstp++ = bswap_16(((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19);
#else
                            *dstp++ = ((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19;
#endif
                        }
                    }
                } else
                {
                    while (width--)
                    {
                        uint32_t icol = *srcp++;
#if BYTE_ORDER == LITTLE_ENDIAN
                        *dstp++ = bswap_16(((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19);
#else
                        *dstp++ = ((icol & 0xFF) >> 3) << 11 | ((icol & 0xFF00) >> 10) << 5 | (icol & 0xFF0000) >> 19;
#endif
                    }
                }
                srcptr+=m_surface->stride;
                dstptr+=surface->stride;
            }
        }
        else {
            printf("[uPNG] cannot blit %dbpp from %dbpp\n", surface->bpp, m_surface->bpp);
            return -1;
        }
    }
    return 0;
}

int uPNG::render(const char* filename, int posX, int posY, gUnmanagedSurface* surface, int width, int height, int bpp, int flag)
{

    m_surface = loadPNG(filename);
	if (m_surface == NULL)
		return -1;

    return blit(surface, width, height, eRect(0,0,width,height), flag);
}


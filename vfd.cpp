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

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <cstring>

#include "vfd.h"

const char *OLED_PROC_1 = "/proc/stb/lcd/oled_brightness";
const char *OLED_PROC_2 = "/proc/stb/fp/oled_brightness";


VFD::VFD()
{
    int xres = 400, yres = 240, bpp = 32;
    flipped = false;
    inverted = 0;
    lcd_type = 0;

    lcdfd = -1;

    if (access(OLED_PROC_1, W_OK) == 0)
        m_oled_brightness_proc = 1;
    else if (access(OLED_PROC_2, W_OK) == 0)
        m_oled_brightness_proc = 2;
    else
        m_oled_brightness_proc = 0;

//    printf("[VFD] m_oled_brightness_proc = %d\n", m_oled_brightness_proc);
    
    lcdfd = open("/dev/dbox/oled0", O_RDWR);

    if (lcdfd < 0)
    {
        if (m_oled_brightness_proc != 0)
            lcd_type = 2;
        lcdfd = open("/dev/dbox/lcd0", O_RDWR);
    }
    else
    {
        printf("[VFD] found OLED display!\n");
        lcd_type = 1;
    }

    if (lcdfd < 0) {
        printf("[VFD] No oled0 or lcd0 device found!\n");
    }
    else
    {

#ifndef LCD_IOCTL_ASC_MODE
#define LCDSET 0x1000
#define LCD_IOCTL_ASC_MODE (21 | LCDSET)
#define LCD_MODE_ASC 0
#define LCD_MODE_BIN 1
#endif

        int i = LCD_MODE_BIN;
        ioctl(lcdfd, LCD_IOCTL_ASC_MODE, &i);
        FILE *f = fopen("/proc/stb/lcd/xres", "r");
        if (f)
        {
            int tmp;
            if (fscanf(f, "%x", &tmp) == 1)
                xres = tmp;
            fclose(f);
            f = fopen("/proc/stb/lcd/yres", "r");
            if (f)
            {
                if (fscanf(f, "%x", &tmp) == 1)
                    yres = tmp;
                fclose(f);
                f = fopen("/proc/stb/lcd/bpp", "r");
                if (f)
                {
                    if (fscanf(f, "%x", &tmp) == 1)
                        bpp = tmp;
                    fclose(f);
                }
            }
            lcd_type = 3;
        }
//        printf("[VFD] xres=%d, yres=%d, bpp=%d lcd_type=%d\n", xres, yres, bpp, lcd_type);
    }
    
    _stride = xres * bpp / 8;
    _buffer = new unsigned char[xres * yres * bpp / 8];
#ifdef LCD_DM900_Y_OFFSET
    xres -= LCD_DM900_Y_OFFSET;
#endif
    res = eSize(xres, yres);
    memset(_buffer, 0, xres * yres * bpp / 8);
//    printf("[VFD] (%dx%dx%d) buffer %p %d bytes, stride %d\n", xres, yres, bpp, _buffer, xres * yres * bpp / 8, _stride);

    m_bpp = bpp;
}


VFD::~VFD()
{
    if (lcdfd >= 0)
    {
        close(lcdfd);
        lcdfd = -1;
    }
    if (_buffer)
        delete[] _buffer;
}

void VFD::Write()
{
#if !defined(HAVE_TEXTLCD) && !defined(HAVE_7SEGMENT)
    if (lcdfd >= 0)
    {
        size_t bs = 0;
        size_t bw = 0;
        
        if (lcd_type == 0 || lcd_type == 2)
        {
            unsigned char raw[132 * 8];
            int x, y, yy;
            for (y = 0; y < 8; y++)
            {
                for (x = 0; x < 132; x++)
                {
                    int pix = 0;
                    for (yy = 0; yy < 8; yy++)
                    {
                        pix |= (_buffer[(y * 8 + yy) * 132 + x] >= 108) << yy;
                    }
                    if (flipped)
                    {
                        /* 8 pixels per byte, swap bits */
#define BIT_SWAP(a) ((((a << 7) & 0x80) + ((a << 5) & 0x40) + ((a << 3) & 0x20) + ((a << 1) & 0x10) + ((a >> 1) & 0x08) + ((a >> 3) & 0x04) + ((a >> 5) & 0x02) + ((a >> 7) & 0x01)) & 0xff)
                        raw[(7 - y) * 132 + (131 - x)] = BIT_SWAP(pix ^ inverted);
                    }
                    else
                    {
                        raw[y * 132 + x] = pix ^ inverted;
                    }
                }
            }
            bs = 132 * 8;
            bw = write(lcdfd, raw, bs);
        }
        else if (lcd_type == 3)
        {
            bs = _stride * res.height();
            /* for now, only support flipping / inverting for 8bpp displays */
            if ((flipped || inverted) && _stride == res.width())
            {
                unsigned int height = res.height();
                unsigned int width = res.width();
                unsigned char raw[_stride * height];
                for (unsigned int y = 0; y < height; y++)
                {
                    for (unsigned int x = 0; x < width; x++)
                    {
                        if (flipped)
                        {
                            /* 8bpp, no bit swapping */
                            raw[(height - 1 - y) * width + (width - 1 - x)] = _buffer[y * width + x] ^ inverted;
                        }
                        else
                        {
                            raw[y * width + x] = _buffer[y * width + x] ^ inverted;
                        }
                    }
                }
                bw = write(lcdfd, raw, bs);
            }
            else
            {
#if defined(LCD_DM900_Y_OFFSET)
                unsigned char gb_buffer[_stride * res.height()];
                for (int offset = 0; offset < ((_stride * res.height()) >> 2); offset++)
                {
                    unsigned int src = 0;
                    if (offset % (_stride >> 2) >= LCD_DM900_Y_OFFSET)
                        src = ((unsigned int *)_buffer)[offset - LCD_DM900_Y_OFFSET];
                    //                                             blue                         red                  green low                     green high
                    ((unsigned int *)gb_buffer)[offset] = ((src >> 3) & 0x001F001F) | ((src << 3) & 0xF800F800) | ((src >> 8) & 0x00E000E0) | ((src << 8) & 0x07000700);
                }
                bw = write(lcdfd, gb_buffer, bs);
#elif defined(LCD_COLOR_BITORDER_RGB565)
                // gggrrrrrbbbbbggg bit order from memory
                // gggbbbbbrrrrrggg bit order to LCD
                unsigned char gb_buffer[_stride * res.height()];
                if (!(0x03 & (_stride * res.height())))
                { // fast
                    for (int offset = 0; offset < ((_stride * res.height()) >> 2); offset++)
                    {
                        unsigned int src = ((unsigned int *)_buffer)[offset];
                        ((unsigned int *)gb_buffer)[offset] = src & 0xE007E007 | (src & 0x1F001F00) >> 5 | (src & 0x00F800F8) << 5;
                    }
                }
                else
                { // slow
                    for (int offset = 0; offset < _stride * res.height(); offset += 2)
                    {
                        gb_buffer[offset] = (_buffer[offset] & 0x07) | ((_buffer[offset + 1] << 3) & 0xE8);
                        gb_buffer[offset + 1] = (_buffer[offset + 1] & 0xE0) | ((_buffer[offset] >> 3) & 0x1F);
                    }
                }
                bw = write(lcdfd, gb_buffer, bs);
#else
                bw = write(lcdfd, _buffer, bs);
#endif
            }
        }
        else /* lcd_type == 1 */
        {
            unsigned char raw[64 * 64];
            int x, y;
            memset(raw, 0, 64 * 64);
            for (y = 0; y < 64; y++)
            {
                int pix = 0;
                for (x = 0; x < 128 / 2; x++)
                {
                    pix = (_buffer[y * 132 + x * 2 + 2] & 0xF0) | (_buffer[y * 132 + x * 2 + 1 + 2] >> 4);
                    if (inverted)
                        pix = 0xFF - pix;
                    if (flipped)
                    {
                        /* device seems to be 4bpp, swap nibbles */
                        unsigned char byte;
                        byte = (pix >> 4) & 0x0f;
                        byte |= (pix << 4) & 0xf0;
                        raw[(63 - y) * 64 + (63 - x)] = byte;
                    }
                    else
                    {
                        raw[y * 64 + x] = pix;
                    }
                }
            }
            bs = 64 * 64;
            bw = write(lcdfd, raw, bs);
        }
//        printf("[VFD] %ld bytes writen %ld\n", bw, bs);

    }
#endif
}

int VFD::setLCDBrightness(int brightness)
{
    if (lcdfd < 0)
        return 0;

    FILE *f = NULL;
    if (m_oled_brightness_proc == 1)
        f = fopen(OLED_PROC_1, "w");
    else if (m_oled_brightness_proc == 2)
        f = fopen(OLED_PROC_2, "w");

    if (f)
    {
        if (fprintf(f, "%d", brightness) == 0)
            printf("[VFD] write oled_brightness failed!! (%m)\n");
        fclose(f);
    }
    return 0;
}

int VFD::displayPNG(const char* filepath, int posX, int posY)
{
    unsigned int height = res.height();
    unsigned int width = res.width();

    surface.x = size().width();
    surface.y = size().height();
    surface.stride = _stride;
    surface.bypp = surface.stride / surface.x;
    surface.bpp = surface.bypp*8;
    surface.data = buffer();
    surface.data_phys = 0;
    if (lcd_type == 4)
    {
        surface.clut.colors = 256;
        surface.clut.data = new gRGB[surface.clut.colors];
        memset(static_cast<void*>(surface.clut.data), 0, sizeof(*surface.clut.data)*surface.clut.colors);
    }
    else
    {
        surface.clut.colors = 0;
        surface.clut.data = 0;
    }

    int res;
	res = m_png.render( filepath, posX, posY, &surface, width, height, m_bpp, 2);

    if(res == 0) {
        Write();
        setLCDBrightness(102);
    }
    
	return res;
}


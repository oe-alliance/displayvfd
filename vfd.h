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

#ifndef _VFD_H_
#define _VFD_H_

#include <string>
//#include "ft.h"
#include "upng.h"
#include "esize.h"

class VFD
{
private:
    int lcdfd;
    unsigned char *_buffer;
    int _stride;
    eSize res;
    unsigned char inverted;
    bool flipped;
	int m_bpp;
	uPNG m_png;
    int lcd_type;
    int m_oled_brightness_proc;
    gUnmanagedSurface surface;

public:

    uint8_t *buffer() {
        return (uint8_t *)_buffer;
    };

    eSize size() { return res; };

    VFD();
	~VFD();
	void Write(void);
	int displayPNG(const char* filepath, int posX, int posY);
    int setLCDBrightness(int brightness);
};

#endif

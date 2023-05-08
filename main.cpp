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
#include <stdlib.h>
#include <string>

#include "vfd.h"

void usage()
{
	
	std::string message = "Usage: displayvfd [option] arg .. &\n";
	message += "Options : \n";
//    message += "    -p [PNG_FILE_PATH] -x [posX] -y [posY]\n";
    message += "	-p [PNG_FILE_PATH]\n";
    printf("%s\n",message.c_str());
}

int main(int argc, char **argv) {

	std::string  fileName;
	int x, y, opt;

	x = 0;
	y = 0;
	opterr	= 0;

	if (argc == 1)
	{
		usage(); return 0;
	}

	while( ( opt = getopt( argc, argv, "p:x:y:")) != -1 )
	{
		switch(opt)
		{
			case 'p':
				fileName = optarg; break;
			case 'x':
				x = atoi(optarg); break;
			case 'y':
				y = atoi(optarg); break;
			default:
				usage(); return 0; break;
		}
	}

    int res = -1;
    VFD * vfd;
    vfd = new VFD();
    if (fileName.size() != 0)
    {
        res = vfd->displayPNG(fileName.c_str(), x, y);
    }
    delete vfd;

	return 0;

}



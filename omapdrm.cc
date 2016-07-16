#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "omapdrm.h"
#include "udev++.h"
#include "die.h"

int omapdrm_open()
{
	// "The whole drmOpen thing is a fiasco and we need to find a way back
	// to just using open(2)." -- libdrm/xf86.c
	//
	// as you wish...
	//
	UdevEnumerate enumerate { "drm" };
	enumerate.match_property( "ID_PATH", "platform-omapdrm.0" );

	for( char const *syspath : enumerate ) {
		UdevDevice dev { syspath };
		if( ! dev )
			continue;

		char const *path = dev.devpath();
		if( ! path || memcmp( path, "/dev/dri/card", 13 ) )
			continue;

		int fd = open( path, O_RDWR | O_CLOEXEC );
		if( fd < 0 )
			die( "open %s: (%d) %m\n", path, -errno );
		return fd;
	}

	die( "can't find omapdrm device\n" );
}

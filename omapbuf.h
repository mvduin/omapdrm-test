#pragma once
#include "omapdrm.h"
#include "die.h"
#include <errno.h>

#ifndef OMAP_BO_FORCE
#define OMAP_BO_FORCE 0x10
#endif

namespace drm {

enum class MemType {
	cached	= 0,  // not supported yet
	normal	= 2,
	device	= 4,
	sync	= 6,
};

struct Buffer {
	__u16 width;
	__u16 height;
	__u8  bpp;  // bytes, not bits
	__u16 stride = 0;

	struct omap_bo *bo = NULL;
	__u32 offset = 0;

	Buffer( __u16 width, __u16 height, __u8  bpp )
		: width( width ), height( height ), bpp( bpp ) {}

	void free()
	{
		omap_bo_del( bo );
		bo = NULL;
	}

	~Buffer() {  free();  }

	void allocate( int fd, MemType mt, bool tiled )
	{
		free();

		struct omap_device *dev = omap_device_new(fd);
		unsigned flags = (unsigned)mt | OMAP_BO_FORCE;
		stride = width * bpp;
		if( tiled ) {
			if( bpp == 1 )
				flags |= OMAP_BO_TILED_8;
			else if( bpp == 2 )
				flags |= OMAP_BO_TILED_16;
			else if( bpp == 4 )
				flags |= OMAP_BO_TILED_32;
			else
				die( "invalid bpp for tiled memory\n" );
			stride += -stride % 4096;
			bo = omap_bo_new_tiled( dev, width, height, flags );
		} else {
			// align to 64 bytes for consistent behaviour
			stride += -stride % 64;
			bo = omap_bo_new( dev, stride * height, flags );
		}
		offset = 0;
		omap_device_del( dev );  // bo retains a reference
		if( ! bo )
			die( "failed to allocate buffer: (%d) %m\n", -errno );
	}

	void allocate( Buffer const *parent, __u16 x, __u16 y )
	{
		free();

		stride = parent->stride;
		bpp = parent->bpp;
		offset = parent->offset + x * bpp + y * stride;
		bo = omap_bo_ref( parent->bo );
	}

	__u32 handle() const
	{
		return omap_bo_handle( bo );
	}

	__u8 *map() const
	{
		__u8 *base = (__u8 *) omap_bo_map( bo );
		if( ! base )
			die( "failed to mmap buffer: (%d) %m\n", -errno );
		return base + offset;
	}
};


}  // namespace drm

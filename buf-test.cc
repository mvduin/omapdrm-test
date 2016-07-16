#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <arm_neon.h>
#include "omapdrm.h"
#include "die.h"

using u8  = __u8;
using u16 = __u16;
using u32 = __u32;

enum class MemType {
	cached	= 0,  // not supported yet
	normal	= 2,
	device	= 4,
	sync	= 6,
};

static inline constexpr u32 round_up( u32 x, u32 m ) {
	return x + -x % m;
}

struct Buffer {
	u16 width;
	u16 height;
	u8  bpp;  // bytes, not bits
	u16 stride = 0;

	struct omap_bo *bo = NULL;
	u32 offset = 0;

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
		u32 flags = (u32)mt | OMAP_BO_SCANOUT;
		if( tiled ) {
			if( bpp == 1 )
				flags |= OMAP_BO_TILED_8;
			else if( bpp == 2 )
				flags |= OMAP_BO_TILED_16;
			else if( bpp == 4 )
				flags |= OMAP_BO_TILED_32;
			else
				die( "invalid bpp for tiled memory\n" );
			stride = round_up( width * bpp, 4096 );
			bo = omap_bo_new_tiled( dev, width, height, flags );
		} else {
			// align to 64 bytes for consistent behaviour
			stride = round_up( width * bpp, 64 );
			bo = omap_bo_new( dev, stride * height, flags );
		}
		offset = 0;
		omap_device_del( dev );  // bo retains a reference
		if( ! bo )
			die( "failed to allocate buffer: (%d) %m\n", -errno );
	}

	void allocate( Buffer const *parent, u16 x, u16 y )
	{
		free();

		stride = parent->stride;
		bpp = parent->bpp;
		offset = parent->offset + x * bpp + y * stride;
		bo = omap_bo_ref( parent->bo );
	}

	u32 handle() const
	{
		return omap_bo_handle( bo );
	}

	u8 *map() const
	{
		u8 *base = (u8 *) omap_bo_map( bo );
		if( ! base )
			die( "failed to mmap buffer: (%d) %m\n", -errno );
		return base + offset;
	}
};

[[gnu::noinline]]
static u32 now() {
	timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (u32)ts.tv_sec * 1000000u + (u32)ts.tv_nsec / 1000u;
}

static char const *const mtname[4] = {
	"cached",
	"normal",
	"device",
	"sync",
};

template< typename T >
inline T read_once( T volatile &obj ) {  return obj;  }

template< typename T >
inline void write_once( T volatile &obj, T value ) {  obj = value;  }

// this is yucky but need to make sure it uses 64-byte vldm/vstm
template<>
inline uint32x4x4_t read_once( uint32x4x4_t volatile &obj )
{
	register uint32x4_t q0 asm("q0");
	register uint32x4_t q1 asm("q1");
	register uint32x4_t q2 asm("q2");
	register uint32x4_t q3 asm("q3");
	asm volatile( "vldm %m4, { %q0,%q1,%q2,%q3 }"
			: "=w"(q0), "=w"(q1), "=w"(q2), "=w"(q3) : "m"(obj) );
	return { q0, q1, q2, q3 };
}

template<>
inline void write_once( uint32x4x4_t volatile &obj, register uint32x4x4_t v )
{
#if 0
	// this doesn't work when inlined
	asm volatile( "vstm %m0, { %q1,%q2,%q3,%q4 }"
		: "=m"(obj)
		: "w"(v.val[0]), "w"(v.val[1]), "w"(v.val[2]), "w"(v.val[3]) );
#else
	// this works but it tends to cause pointless vmovs
	register uint32x4_t q0 asm("q0") = v.val[0];
	register uint32x4_t q1 asm("q1") = v.val[1];
	register uint32x4_t q2 asm("q2") = v.val[2];
	register uint32x4_t q3 asm("q3") = v.val[3];
	asm volatile( "vstm %m0, { %q1,%q2,%q3,%q4 }"
			: "=m"(obj) : "w"(q0), "w"(q1), "w"(q2), "w"(q3) );
#endif
}

template< typename T >
static void fill_test( u8 *p, uint n )
{
	auto const line = (T volatile *) p;

	n *= 0x1000;

	for( uint j = 0; j < n; j++ )
		for( uint x = 0; x < 4096 / sizeof(T); x++ )
			write_once( line[x], T {} );
}

template< typename T >
static void read_test( u8 *p, uint n )
{
	auto const line = (T volatile *) p;

	n *= 0x1000;

	for( uint j = 0; j < n; j++ )
		for( uint x = 0; x < 4096 / sizeof(T); x++ )
			read_once( line[x] );
}


[[gnu::noinline]]
static void timeit( u8 *p, uint n, void (*proc)( u8 *, uint ) )
{
	for( uint k = 0; k < 3; k++ ) {
		u32 t = now();
		proc( p, n );
		u32 t2 = now();
		proc( p, n*2 );
		u32 t3 = now();
		t = (t3 - t2) - (t2 - t);
		printf( "%6.1f MB/s\n", n * 16e6 / t );
	}
}

static void buffer_test( int fd, MemType mt, bool tiled )
{
	Buffer buf { 1280, 720, 4 };
	buf.allocate( fd, mt, tiled );

	u8 *const p = buf.map();

	printf( "---- %stiled (%s) ----\n",
			tiled ? "" : "non-", mtname[(uint)mt / 2] );
	printf( "fill u32:\n" );	timeit( p, 4, fill_test<u32> );
	printf( "fill u32x4:\n" );	timeit( p, 4, fill_test<uint32x4_t> );
	printf( "fill u32x4x4:\n" );	timeit( p, 4, fill_test<uint32x4x4_t> );
	if( tiled && mt == MemType::normal )
		return;  // let's not do bus errors
	printf( "read u32:\n" );	timeit( p, 2, read_test<u32> );
	printf( "read u32x4:\n" );	timeit( p, 2, read_test<uint32x4_t> );
	printf( "read u32x4x4:\n" );	timeit( p, 2, read_test<uint32x4x4_t> );
};

int main( int argc, char **argv )
{
	int fd = omapdrm_open();

#if 0
	// these are just pointlessly slow
	buffer_test( fd, MemType::sync,   false );
	buffer_test( fd, MemType::sync,   true );
#endif
	buffer_test( fd, MemType::device, false );
	buffer_test( fd, MemType::device, true );
	buffer_test( fd, MemType::normal, false );
	buffer_test( fd, MemType::normal, true );

	close( fd );
	return 0;
}

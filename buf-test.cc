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
#include "omapbuf.h"
#include "die.h"

using namespace drm;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

[[gnu::noinline]]
static u32 now() {
	timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (u32)ts.tv_sec * 1000000u + (u32)ts.tv_nsec / 1000u;
}


template< typename T >
inline void write_once( T &obj, T value ) {
	obj = value;
	asm volatile ( "" : "+m"(obj) );
}

template<>
inline void write_once( uint32x4_t &obj, uint32x4_t value )
{
	vst1q_u32( (u32 *) &obj, value );
	asm volatile ( "" : "+m"(obj) );
}

inline void read_once( u32 &obj )
{
	asm volatile ( "" : "+m"(obj) );
	asm volatile ( "" :: "r"(obj) );
}

inline void read_once( uint32x4_t &obj )
{
	asm volatile ( "" : "+m"(obj) );
	asm volatile ( "" :: "w"(obj) );
}

inline void read_once( uint32x4x4_t &obj )
{
	asm volatile( "vldm %m0, { q0-q3 }" :: "m"(obj) : "q0", "q1", "q2", "q3" );
}

template< typename T >
static void fill_test( u8 *p, uint n )
{
	auto const line = (T *) p;

	n *= 0x1000;

	for( uint j = 0; j < n; j++ )
		for( uint x = 0; x < 4096 / sizeof(T); x++ )
			write_once( line[x], T {} );
}

template< typename T >
static void read_test( u8 *p, uint n )
{
	auto const line = (T *) p;

	n *= 0x1000;

	for( uint j = 0; j < n; j++ )
		for( uint x = 0; x < 4096 / sizeof(T); x++ )
			read_once( line[x] );
}


[[gnu::noinline]]
static void timeit( u8 *p, uint n, void (*proc)( u8 *, uint ) )
{
	uint m = 4;
	u32 t[m];
	// hide count from optimizer
	asm( "" : "+r"(m) :: "memory" );
	for( uint k = 0; k < m; k++ ) {
		proc( p, n );
		t[k] = now();
		asm( "" ::: "memory" );
	}
	for( uint k = 1; k < m; k++ ) {
		printf( "\t%6.1f MB/s", n * 16e6 / (t[k] - t[k-1]) );
	}
	printf( "\n" );
}

static char const *const mtname[4] = {
	"cached",
	"normal",
	"device",
	"sync",
};

static void run_fills( int fd, MemType mt, bool tiled )
{
	Buffer buf { 1280, 720, 4 };
	buf.allocate( fd, mt, tiled );
	u8 *const p = buf.map();

	printf( "\n---- %stiled (%s) ----\n",
			tiled ? "" : "non-", mtname[(uint)mt / 2] );
	printf( "write (str):" );	timeit( p, 2, fill_test<u32> );
	printf( "write (vst1q):" );	timeit( p, 2, fill_test<uint32x4_t> );
	printf( "write (vstmqq):" );	timeit( p, 2, fill_test<uint32x4x4_t> );
};

static void run_reads( int fd, MemType mt, bool tiled )
{
	Buffer buf { 1280, 720, 4 };
	buf.allocate( fd, mt, tiled );
	u8 *const p = buf.map();

	printf( "\n---- %stiled (%s) ----\n",
			tiled ? "" : "non-", mtname[(uint)mt / 2] );
	printf( "read (ldr):" );	timeit( p, 1, read_test<u32> );
	printf( "read (vld1q):" );	timeit( p, 1, read_test<uint32x4_t> );
	printf( "read (vldmqq):" );	timeit( p, 1, read_test<uint32x4x4_t> );
};

int main( int argc, char **argv )
{
	int fd = omapdrm_open();

	// note: MemType::sync is pointlessly slow and the option is invalid on
	// mainline linux (although there MemType::device actually gets you sync)

	printf( "================ write tests ===============================\n\n" );
	printf( "reference points:\n" );
	printf( "\t1280 x  720 x 32-bit x 60 fps =  211 MB/s\n" );
	printf( "\t1920 x 1080 x 32-bit x 60 fps =  475 MB/s\n" );
	printf( "\traw bandwidth to L3 intercon  = 2021 MB/s (per direction)\n" );
	printf( "\traw bandwidth DDR3 memory     = 4066 MB/s\n" );
//	run_fills( fd, MemType::sync,   false );
//	run_fills( fd, MemType::sync,   true );
	run_fills( fd, MemType::device, false );
	run_fills( fd, MemType::device, true );
	run_fills( fd, MemType::normal, false );
	run_fills( fd, MemType::normal, true );

	printf( "\n" );
	printf( "================ read tests ================================\n\n" );
	printf( "(of course you should try to avoid reading a framebuffer anyway)\n" );
//	run_reads( fd, MemType::sync,   false );
//	run_reads( fd, MemType::sync,   true );
	run_reads( fd, MemType::device, false );
	run_reads( fd, MemType::device, true );
	run_reads( fd, MemType::normal, false );

	close( fd );
	return 0;
}

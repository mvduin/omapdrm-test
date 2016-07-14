#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void warn( char const *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
}

[[ noreturn ]]
static void die( char const *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	exit( EXIT_FAILURE );
}

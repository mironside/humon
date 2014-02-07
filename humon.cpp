#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include "jsmn.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MAX_TOKENS 4096

int main( int argc, char **argv )
{
	jsmntok_t *value;
	FILE *f;
	int length;
	char *buffer;

	if ( argc < 2 ) {
		printf( "usage: humon.exe <filename>\n" );
		return 0;
	}

	// @todo: memory map?
	f = fopen( argv[1], "rb" );
	if ( !f ) {
		printf( "File Not Found.\n" );
		return 0;
	}

	fseek( f, 0, SEEK_END );
	length = ftell( f );
	fseek( f, 0, SEEK_SET );
	buffer = (char *)malloc( length );
	fread( buffer, length, 1, f );


	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
	LARGE_INTEGER end;

	QueryPerformanceFrequency( &frequency );
	QueryPerformanceCounter( &start );

	jsmnerr_t err = jsmn_parse( buffer, length, &value );

	QueryPerformanceCounter( &end );

	double dt = double(end.QuadPart - start.QuadPart) / double(frequency.QuadPart);

	if ( err != JSMN_SUCCESS ) {
		printf( "Failed!\n" );
	} else {
		int size;
		int count;
		jsmn_allocations( &count, &size );
		printf( "Success!\n%d Buffer Bytes\n%d Allocations\n%d Bytes Allocated\n%f Seconds\n", length, count, size, dt );
	}
}

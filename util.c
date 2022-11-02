/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

void
report(int err, const char *title, const char *fmt, ...)
{
    FILE *stream;
    va_list ap;
//    time_t rawtime;

    stream = ((err) ? stderr : stdout);

//    time(&rawtime);
//    fprintf(stream, "[%s] <%li> ", title, rawtime);

    fprintf(stream, "[%s] ", title);
    
    va_start(ap, fmt);
    vfprintf(stream, fmt, ap);
    va_end(ap);

    fputc('\n', stream);
}

void
die(const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    report(1, "ERROR", fmt, ap);
    va_end(ap);
    
    exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
    void *p;

    if (!(p = calloc(nmemb, size)))
	die("calloc:");
    return p;
}


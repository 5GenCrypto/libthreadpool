lib_LTLIBRARIES = libthreadpool.la

libthreadpool_la_LDFLAGS= -no-undefined -version-info 0:0:0
libthreadpool_la_CFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS)
libthreadpool_la_LIBADD= -lpthread

libthreadpool_la_SOURCES = threadpool.c

include_HEADERS = threadpool.h

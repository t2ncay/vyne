// SPDX-License-Identifier: MIT
#include "../vyne_runtime.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
static int fail_write, fail_close, fail_replace, fail_fdopen, open_streams, last_fd=-1;
static char temporary_path[4096];
static FILE *temporary_stream;
static size_t test_fwrite(const void *p,size_t size,size_t count,FILE *f) {
    if(fail_write) { fail_write=0; return fwrite(p,size,count/2,f); }
    return fwrite(p,size,count,f);
}
static int test_fclose(FILE *f) {
    if(f==temporary_stream) { temporary_stream=NULL; --open_streams; }
    int result=fclose(f);
    if(fail_close) { fail_close=0; return EOF; }
    return result;
}
static FILE *test_fdopen(int fd,const char *mode) {
    last_fd=fd;
    if(fail_fdopen) { fail_fdopen=0; return NULL; }
#ifdef _WIN32
    FILE *f=_fdopen(fd,mode);
#else
    FILE *f=fdopen(fd,mode);
#endif
    if(f) { temporary_stream=f; ++open_streams; }
    return f;
}
#ifdef _WIN32
static int test_open(const char *path,int flags,int mode) {
    assert(strlen(path)<sizeof(temporary_path)); strcpy(temporary_path,path);
    return _open(path,flags,mode);
}
static BOOL test_replace(LPCSTR from,LPCSTR to,DWORD flags) {
    if(fail_replace) { fail_replace=0; SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return MoveFileExA(from,to,flags);
}
#define _open test_open
#define _fdopen test_fdopen
#define MoveFileExA test_replace
#else
static int test_mkstemp(char *path) {
    int fd=mkstemp(path);
    assert(strlen(path)<sizeof(temporary_path)); strcpy(temporary_path,path);
    return fd;
}
static int test_replace(const char *from,const char *to) {
    if(fail_replace) { fail_replace=0; errno=EACCES; return -1; }
    return rename(from,to);
}
#define mkstemp test_mkstemp
#define fdopen test_fdopen
#define rename test_replace
#endif
#define fwrite test_fwrite
#define fclose test_fclose
#include "../modules/vfs.h"
static void retained(VyneValue path) {
    VyneValue value=vfs_read(path);
    assert(value.type==V_STRING && !strcmp(value.as.str,"original"));
    assert(!vfs_exists(vyne_string_static(temporary_path)).as.i64);
    assert(open_streams==0 && temporary_stream==NULL);
}
int main(int argc,char **argv) {
    assert(argc==2);
    VyneValue path=vyne_string_static(argv[1]);
    assert(vfs_write(path,vyne_string_static("original")).as.i64);
#ifdef _WIN32
    // Exclusive sibling creation must not overwrite a colliding temp file.
    char collision[4096];
    snprintf(collision,sizeof(collision),"%s.vyne-tmp-%lu-1",argv[1],(unsigned long)GetCurrentProcessId());
    assert(vfs_write(vyne_string_static(collision),vyne_string_static("sentinel")).as.i64);
    assert(vfs_write_atomic(path,vyne_string_static("original")).as.i64);
    assert(!strcmp(vfs_read(vyne_string_static(collision)).as.str,"sentinel"));
    assert(vfs_remove(vyne_string_static(collision)).as.i64);
#endif
    fail_write=1;
    assert(!vfs_write_atomic(path,vyne_string_static("replacement")).as.i64); retained(path);
    fail_close=1;
    assert(!vfs_write_atomic(path,vyne_string_static("replacement")).as.i64); retained(path);
    fail_replace=1;
    assert(!vfs_write_atomic(path,vyne_string_static("replacement")).as.i64); retained(path);
    fail_fdopen=1;
    assert(!vfs_write_atomic(path,vyne_string_static("replacement")).as.i64); retained(path);
#ifdef _WIN32
    assert(_get_osfhandle(last_fd)==-1);
#else
    assert(fcntl(last_fd,F_GETFD)==-1 && errno==EBADF);
#endif
    // Ordinary write must report a failed flush/close too.
    fail_close=1;
    assert(!vfs_write(path,vyne_string_static("changed")).as.i64);
    assert(vfs_write_atomic(path,vyne_string_static("original")).as.i64); retained(path);
    assert(vfs_remove(path).as.i64);
    fail_replace=1;
    assert(!vfs_write_atomic(path,vyne_string_static("new")).as.i64);
    assert(!vfs_exists(path).as.i64 && !vfs_exists(vyne_string_static(temporary_path)).as.i64);
    assert(vfs_write_atomic(path,vyne_string_static("created")).as.i64);
    assert(!strcmp(vfs_read(path).as.str,"created"));
    assert(vfs_write_atomic(path,vyne_string_static("")).as.i64 && vfs_size(path).as.i64==0);
    assert(vfs_remove(path).as.i64);
    assert(open_streams==0);
    arena_free_all();
    puts("VFS_ATOMIC_FAILURE_OK");
}

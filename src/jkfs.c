// TODO: define not finished
#define FUSE_USE_VERSION 
#define _XOPEN_SOURCE 

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <assert.h>

#define MAXPATH 256
static char SSDPATH[MAXPATH];
static char HDDPATH[MAXPATH];
static char MP[MAXPATH];
static size_t THRESH;
static unsigned long count = 0;

static struct fuse_operations jk_ops = {
	.getattr	= jk_getattr,
	.access		= jk_access,
	.readlink	= jk_readlink,


};

int main()
{
    printf("Hello world\n");
    return 0;
}


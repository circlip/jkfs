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
	.getdir		= jk_getdir,
	.mknod		= jk_mknod,
	.mkdir		= jk_mkdir,
	.unlink		= jk_unlink,
	.rmdir		= jk_rmdir,
	.unlink		= jk_unlink,
	.rmdir		= jk_rmlink,
	.symlink	= jk_symlink, 
	.rename		= jk_rename,
	.link		= jk_link,
	.chmod		= jk_chmod,
	.chown		= jk_chown,
	.truncate	= jk_chuncate,
	.utime		= jk_utime,
	.open		= jk_open,
	.read		= jk_read,
	.write		= jk_write,
	.statfs		= jk_statfs,
	.flush		= jk_flush,
	.release	= jk_release,
	.fsync		= jk_fsync,
	.setxattr	= jk_setxattr,
	.getxattr	= jk_getxattr,
	.listxattr	= jk_listxattr,
	.removexattr= jk_removexattr,
	.opendir	= jk_opendir,
	.readdir	= jk_readdir,
	.releasedir	= jk_releasedir,
	.fsyncdir	= jk_fsyncdir,
	.init		= jk_init,
	.destroy	= jk_destroy,
	.create		= jk_create,
	.ftruncate	= jk_ftruncate,
	.fgetattr	= jk_fgetattr,
	.lock		= jk_lock,
	.utimens	= jk_utimens,
	.bmap		= jk_bmap,
	.ioctl		= jk_ioctl,
	.poll		= jk_poll,
	.write_buf	= jk_write_buf,
	.read_buf	= jk_read_buf,
	.flock		= jk_flock,
	.fallocate	= jk_fallocate,
};

int main()
{
    printf("Hello world\n");
    return 0;
}

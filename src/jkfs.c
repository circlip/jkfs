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


static int jk_create(const char *path, mode_t mode, struct fuse_file_info *info) {

}

static int jk_getattr(const char *path, struct stat *statbuf) {

}

static int jk_access(const char *path, int mask) {

}

static int jk_readlink(const char *path, char* buf, size_t size) {

}

static int jk_readdir(const char *path, char *buf, 
                      fuse_fill_dir_t filler, 
                      off_t offset, struct fuse_file_info *info) {

}

static int jk_mknod(const char *path, mode_t mode, dev_t rdev) {

}

static int jk_mkdir(const char *path, mode_t mode) {

}

static int jk_unlink(const char *path) {

}

static int jk_rmdir(const char *path) {

}

static int jk_symlink(const char *from, const char *to) {

}

static int jk_rename(const char *from, const char *to) {

}

static int jk_chmod(const char *path, mode_t mode) {

}

static int jk_chown(const char* path, uid_t userid, gid_t groupid) {

}

static int jk_truncate(const char *path, off_t size) {

}

static int jk_utimens(const char *path, const struct timespec ts[2]){

}

static int jk_open(const char *path, struct fuse_file_info *info) {

}

static int jk_read(const char *path, char *buf, 
                   size_t size, off_t offset, 
                   struct fuse_file_info *info) {

}

static int jk_write(const char *path, const char *buf, 
                    size_t size, off_t offset, 
                    struct fuse_file_info *info) {

}

static int jk_statfs(const char *path, struct statvfs *statbuf) {

}

static int jk_release(const char *path, struct fuse_file_info *info) {

}

static int jk_fsync(const char *path, int isdatasync) {

}

static int jk_fallocate(const char *path, int mode, 
                        off_t offset, off_t length, 
                        struct fuse_file_info *info) {

}

static int jk_setxattr(const char *path, const char *name, const char *value, 
                       size_t size, int flag) {

}

static int jk_getxattr(const char *path, const char *name, char *value, size_t size) {

}

static int jk_listxattr(const char *path, char *list, size_t size) {

}

static int jk_removexattr(const char *path, const char *name) {

}



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
	.truncate	= jk_truncate ,
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

int read_args_from_file() {
    FILE *fp;
    if ((fp = fopen("args_file", "r")) == NULL) {
        perror("read file error: args_file.\n");
        exit(EXIT_FAILURE);
    }
    fscanf(fd, "%zu %s %s %s", &THRESH, SSDPATH, HDDPATH, MP);
    return;
}

int main()
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    
    // read arguments, mainly ssdpath, hddpath, etc.
    if (argc < 5) {
        read_args_from_file();
        if (argc == 2 && strncmp(argv[1], "-d", 2) == 0) 
            fuse_opt_add_arg(&args, "-d");
    } else if (argc != 5) {
        fputs("error in threshold, ssdpath, hddpath, mountpoint\n", stderr);
        exit(EXIT_FAILURE);
    } else {
        THRES = atoi(argv[1]);
        strcpy(SSDPATH, argv[2]);
        strcpy(HDDPATH, argv[3]);
        strcpy(MP, argv[4]);
    }

    fuse_opt_add_arg(&args, MP);
    return fuses_main(args.argc, args.argv, &jk_ops, NULL);
}











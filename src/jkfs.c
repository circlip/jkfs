#define FUSE_USE_VERSION 29 
#define _GNU_SOURCE
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
#define JK_SUCCESS 0
#define MAXPATH 256
static char SSDPATH[MAXPATH];
static char HDDPATH[MAXPATH];
static char MP[MAXPATH];
static char name[32];
static size_t THRESH;
static unsigned long count = 0;

#ifdef debug
#define print_name  printf("##### %s: ", name)
#endif

/**
* a path must have a coresponding ssdpath
* so must return success
*/
static int path2ssd(const char *path, char *ssdpath) {
    strcpy(ssdpath, SSDPATH);
    strcat(ssdpath, path);
    int res;
    char linkedname[MAXPATH];
    // if ssdpath is a symlink, than parse the link
    // todo: how to create a link to ssd elegantly
    while ((res = readlink(ssdpath, linkedname, MAXPATH - 1)) != -1) {
        linkedname[res] = 0;
        strcpy(strrchr(ssdpath, '/') + 1, linkedname);
    }
    return JK_SUCCESS;
}

/**
* return success if xattrpath exists and set correspondingly
* return -1 if not exists
*/
static int ssd2xattr(const char *ssdpath, char *xattrpath) {
    strcpy(xattrpath, ssdpath);
    sprintf(strrchr(xattrpath, '/') + 1, "%s%s", ".xattr_", strrchr(ssdpath, '/') + 1);
    int res;
    struct stat st;
    res = stat(xattrpath, &st);
    // if not exists
    if (res == -1) {
        return -1;
    }
    // if is not a link
    if (!S_ISLNK(st.st_mode)) {
        return -1;
    }
    return JK_SUCCESS;
}

/**
* return success if hddpath is found
*/
static int xattr2hdd(const char* ssdpath, char* hddpath) {
    char target[MAXPATH];
    int res;
    // todo: when creating link to hdd, how to do it elegantly ?
    res = readlink(xattrpath, target, MAXPATH - 1);
    if (res == -1) {
        return -errno;
    }
    target[res] = 0;
    sprintf(hddpath, "%s/%s", HDDPATH, target);
    return JK_SUCCESS;    
}

static int jk_creat(const char *path, mode_t mode, struct fuse_file_info *info) {
    
}

static int jk_getattr(const char *path, struct stat *stbuf) {
}

static int jk_access(const char *path, int mask) {
}

static int jk_readlink(const char *path, char* buf, size_t size) {
}

static int jk_opendir(const char *path, struct fuse_file_info *info) {
	char ssdpath[MAXPATH];
	path2ssdpath;
	info->fh = (intptr_t)opendir(ssdpath);
	return JK_SUCCESS;
}

static int jk_readdir(const char *path, char *buf, 
						fuse_fill_dir_t filler, off_t offset, 
						struct fuse_file_info *fi) {
	int res = 0;
	DIR *dp;
	struct dirent *de;
	dp = (DIR *)(uintptr_t)fi->fh;
	while ((de = readdir(dp)) != NULL) {
		if (strncmp(de->d_name, ".xattr_", 7) == 0) continue;
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0)) {
			break;	
		}	
	}	
	closedir(dp);
	return JK_SUCCESS;
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
    fprintf(stderr, "Not implemented yet.\n");
    return -1;
}

static int jk_release(const char *path, struct fuse_file_info *info) {
    (void) path;
    (void) info;
    return JK_SUCCESS;
}

static int jk_fsync(const char *path, int isdatasync,
                    struct fuse_file_info *info) {
    (void) path;
    (void) info;
    return JK_SUCCESS;
}


#ifdef HAVE_POSIX_FALLOCATE
static int jk_fallocate(const char *path, int mode, 
                        off_t offset, off_t length, 
                        struct fuse_file_info *info) {
    int fd, res;
    (void) info;
    if (mode) {
        return -EOPNOTSUPP;
    }
    // note: the use of jk_open may be incorrect
    // fd = open(path, O_WRONLY);
    fd = jk_open(path, info);
    if (fd < 0) {
        return -errno;
    }
    res = posix_fallocate(fd, offset, length);
    close(fd);
    return -res;
}
#else
static int jk_fallocate(const char *path, int mode, 
                        off_t offset, off_t length,
                        struct fuse_file_info *info) {
    int fd, res;
    (void) info;
    // note: the use of jk_open may be incorrect
    // fd = open(path, O_WRONLY);
    fd = jk_open(path, info);
    if (fd < 0) {
        return -errno;
    }
    res = fallocate(fd, mode, offset, length);
    if (res < 0) {
        return -errno;
    }
    close(fd);
    return JK_SUCCESS;
}
#endif /* HAVE_POSIX_FALLOCATE */


#ifdef HAVE_SETXATTR
static int jk_setxattr(const char *path, const char *name, const char *value, 
                       size_t size, int flag) {
    int res = lsetxattr(path, name, value, size, flag);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_getxattr(const char *path, const char *name, char *value, size_t size) {
    int res = lgetxattr(path, name, value, size);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_listxattr(const char *path, char *list, size_t size) {
    int res = llistxattr(path, list, size);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_removexattr(const char *path, const char *name) {
    int res = lremovexattr(path, name);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations jk_ops = {
	.getattr	= jk_getattr,
	.access		= jk_access,
	.readlink	= jk_readlink,
	// .getdir		= jk_getdir,
	.mknod		= jk_mknod,
	.mkdir		= jk_mkdir,
	.unlink		= jk_unlink,
	.rmdir		= jk_rmdir,
	// .rmlink		= jk_rmlink,
	.symlink	= jk_symlink, 
	.rename		= jk_rename,
	// .link		= jk_link,
	.chmod		= jk_chmod,
	.chown		= jk_chown,
	.truncate	= jk_truncate ,
	.utimens	= jk_utimens,
	.open		= jk_open,
	.read		= jk_read,
	.write		= jk_write,
	.statfs		= jk_statfs,
	// .flush		= jk_flush,
	.release	= jk_release,
	.fsync		= jk_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= jk_setxattr,
	.getxattr	= jk_getxattr,
	.listxattr	= jk_listxattr,
 	.removexattr= jk_removexattr,
#endif
	.opendir	= jk_opendir,
	.readdir	= jk_readdir,
	// .releasedir	= jk_releasedir,
	// .fsyncdir	= jk_fsyncdir,
	// .init		= jk_init,
	// .destroy	= jk_destroy,
	.create		= jk_creat,
	// .ftruncate	= jk_ftruncate,
    // .fgetattr	= jk_fgetattr,
	// .lock		= jk_lock,
	.utimens	= jk_utimens,
	// .bmap		= jk_bmap,
	// .ioctl		= jk_ioctl,
	// .poll		= jk_poll,
	// .write_buf	= jk_write_buf,
	// .read_buf	= jk_read_buf,
	// .flock		= jk_flock,
	.fallocate	= jk_fallocate,
};

int read_args_from_file() {
    FILE *fp;
    if ((fp = fopen("args_file", "r")) == NULL) {
        perror("args_file");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%zu %s %s %s", &THRESH, SSDPATH, HDDPATH, MP);
    return JK_SUCCESS;
}

int main(int argc, char* argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    
    // read arguments, mainly ssdpath, hddpath, and mountpoint.
    if (argc < 5) {
        read_args_from_file();
        if (argc == 2 && strncmp(argv[1], "-d", 2) == 0) 
            fuse_opt_add_arg(&args, "-d");
    } else if (argc != 5) {
        fputs("error in threshold, ssdpath, hddpath, mountpoint\n", stderr);
        exit(EXIT_FAILURE);
    } else {
        THRESH = atoi(argv[1]);
        strcpy(SSDPATH, argv[2]);
        strcpy(HDDPATH, argv[3]);
        strcpy(MP, argv[4]);
    }

    fuse_opt_add_arg(&args, MP);
    return fuse_main(args.argc, args.argv, &jk_ops, NULL);
}


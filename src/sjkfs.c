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
#define FJK_SUCCESS 0
#define MAXPATH 256
#define BUF_SIZE 131072
static char SSDPATH[MAXPATH];
static char HDDPATH[MAXPATH];
static char MP[MAXPATH];
static char name[32];
static size_t THRESH;
static unsigned long count = 0;


#define path2hdd sprintf(hddpath, "%s%s", HDDPATH, path)

static int jk_getattr(const char *path, struct stat *stbuf) {
	char hddpath[MAXPATH];
	int res;
    path2hdd;
    res = lstat(hddpath, stbuf);
    if (res < 0) {
        return -errno;
    }    
    return FJK_SUCCESS;
}

static int jk_readlink(const char *path, char* buf, size_t size) {

    char hddpath[MAXPATH];
    path2hdd;
    int res;
    res = readlink(hddpath, buf, size - 1);
    if (res == -1) {
        return -errno;
    }
    buf[res] = 0;
    return FJK_SUCCESS;
}


static int jk_mknod(const char *path, mode_t mode, dev_t rdev) {
    char hddpath[MAXPATH];
    path2hdd;
    int res;
    if (S_ISREG(mode)) {
        res = open(hddpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0) {
            close(res);
        }
    } else if (S_ISFIFO(mode)) {
        res = mkfifo(hddpath, mode);
    } else {
        res = mknod(hddpath, mode, rdev);
    }
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_mkdir(const char *path, mode_t mode) {
    // easy. all happen in ssd
    char hddpath[MAXPATH];
    path2hdd;
    int res;
    res = mkdir(hddpath, mode);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

/**
* this remove a file
*/
static int jk_unlink(const char *path) {
    int res;
    char hddpath[MAXPATH];
    path2hdd;
    res = unlink(hddpath);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_rmdir(const char *path) { 
    // easy. all happen in ssdpath
    char hddpath[MAXPATH];
    int res;
    path2hdd;
    res = rmdir(hddpath);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_symlink(const char *path, const char *link) {
    int res;
    char hddpath[MAXPATH], hddlink[MAXPATH];
    path2hdd;
    sprintf(hddlink, "%s%s", HDDPATH, link);
    res = symlink(hddpath, hddlink);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_link(const char *path, const char *newpath) {
    int res;
    char hddpath[MAXPATH], hddnewpath[MAXPATH];
    path2hdd;
    sprintf(hddnewpath, "%s%s", HDDPATH, newpath);
    res = link(hddpath, hddnewpath);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_rename(const char *path, const char *newpath) {
    int res;
    char hddpath[MAXPATH], hddnewpath[MAXPATH];
    path2hdd;
    sprintf(hddnewpath, "%s%s", HDDPATH, newpath);
    res = rename(hddpath, hddnewpath);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_chmod(const char *path, mode_t mode) {
    int res;
    char hddpath[MAXPATH];
	path2hdd;
    res = chmod(hddpath, mode);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;  
}

static int jk_chown(const char* path, uid_t uid, gid_t gid) {
    int res;
    char hddpath[MAXPATH];
	path2hdd;
    res = chown(hddpath, uid, gid);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;  
} 

static int jk_truncate(const char *path, off_t size) {
    int res;
    char hddpath[MAXPATH];
    path2hdd;
    res = truncate(hddpath, size);
    if (res != 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}

// fixme: stupid open read write!
static int jk_open(const char *path, struct fuse_file_info *fi) {
    int fd;
    char hddpath[MAXPATH];
    path2hdd;
    fd = open(hddpath, fi->flags);
    if (fd < 0) {
        perror("open");
        return -errno;
    }
    fi->fh = fd;
    // close(fd);
    return fd;
}

static int jk_read(const char *path, char *buf, 
                   size_t size, off_t offset, 
                   struct fuse_file_info *fi) {
	int res, fd;
	fd = (int)fi->fh;
    res = pread(fd, buf, size, offset);
    if (res < 0) {
        return -errno;
    }
    return res;
}

static int jk_write(const char *path, const char *buf, 
                    size_t size, off_t offset, 
                    struct fuse_file_info *fi) {
    int res, fd;
	fd = (int)fi->fh;
    res = pwrite(fd, buf, size, offset);
    if (res < 0) {
        return -errno;
    }
    return res;
}

static int jk_statfs(const char *path, struct statvfs *statbuf) {
    int res;
    char hddpath[MAXPATH];
    path2hdd;
    res = statvfs(hddpath, statbuf);
    if (res != 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_flush(const char *path, struct fuse_file_info *fi) {
    return FJK_SUCCESS;
}

static int jk_release(const char *path, struct fuse_file_info *fi) {
    int res;
    res = close((int)fi->fh);
    if (res < 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_fsync(const char *path, int isdatasync,
                    struct fuse_file_info *fi) {
    int res;
    if (isdatasync) 
        res = fdatasync((int)fi->fh);
    else
        res = fsync((int)fi->fh);
    if (res < 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}



#ifdef HAVE_SETXATTR
static int jk_setxattr(const char *path, const char *name, const char *value, 
                       size_t size, int flag) {
    char hddpath[MAXPATH];
    path2hdd;
    int res = lsetxattr(hddpath, name, value, size, flag);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_getxattr(const char *path, const char *name, char *value, size_t size) {
    char hddpath[MAXPATH];
    path2hdd;
    int res = lgetxattr(hddpath, name, value, size);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_listxattr(const char *path, char *list, size_t size) {
    char hddpath[MAXPATH];
    path2hdd;
    int res = llistxattr(hddpath, list, size);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_removexattr(const char *path, const char *name) {
    char hddpath[MAXPATH];
    path2hdd;
    int res = lremovexattr(hddpath, name);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}
#endif /* HAVE_SETXATTR */

static int jk_opendir(const char *path, struct fuse_file_info *fi) {
    DIR *dp;
    int res = 0;
    char hddpath[MAXPATH];
    path2hdd;
    dp = opendir(hddpath);
    if (dp == NULL) {
        return -errno;
    }
    fi->fh = (intptr_t)dp;
	return FJK_SUCCESS;
}

static int jk_readdir(const char *path, void *buf, 
						fuse_fill_dir_t filler, off_t offset, 
						struct fuse_file_info *fi) {
	int res = 0;
	DIR *dp;
	struct dirent *de;
	dp = (DIR *)(uintptr_t)fi->fh;
	while ((de = readdir(dp)) != NULL) {
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
	}
	return FJK_SUCCESS;
}

static int jk_releasedir(const char *path, struct fuse_file_info *fi) {
    int res = 0;
    res = closedir((DIR*)(uintptr_t)fi->fh);
    if (res < 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_access(const char *path, int mask) {
    char hddpath[MAXPATH];
    path2hdd;
    int res;
    res = access(hddpath, mask);
    if (res != 0) {
        return -errno;
    }
    return FJK_SUCCESS;
}

static int jk_creat(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char hddpath[MAXPATH];
    // int res;
    path2hdd;
	// res = creat(hddpath, mode);
	int fd = open(hddpath, fi->flags, mode);
    if (fd < 0) {
        return -errno;
    }
	fi->fh = fd;
    return FJK_SUCCESS;
}

static int jk_utimens(const char *path, const struct timespec ts[2]){
    int res;
    char hddpath[MAXPATH];
    path2hdd;
    res = utimensat(0, hddpath, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1) {
        return -errno;
    }
    return FJK_SUCCESS;
}

#ifdef HAVE_POSIX_FALLOCATE
static int jk_fallocate(const char *path, int mode, 
                        off_t offset, off_t length, 
                        struct fuse_file_info *fi) {
    int fd, res;
    (void) fi;
    if (mode) {
        return -EOPNOTSUPP;
    }
    fd = jk_open(path, fi);
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
                        struct fuse_file_info *fi) {
    int fd, res;
    (void) fi;
    res = jk_open(path, fi);
    if (res < 0) {
        return -errno;
    }
    fd = (int)fi->fh;
    res = fallocate(fd, mode, offset, length);
    if (res < 0) {
        return -errno;
    }
    close(fd);
    return FJK_SUCCESS;
}
#endif /* HAVE_POSIX_FALLOCATE */

static struct fuse_operations jk_ops = {
	.getattr	= jk_getattr,
	.readlink	= jk_readlink,
	// .getdir		= jk_getdir,
	.mknod		= jk_mknod,
	.mkdir		= jk_mkdir,
	.unlink		= jk_unlink,
	.rmdir		= jk_rmdir,
	// .rmlink		= jk_rmlink,
	.symlink	= jk_symlink, 
	.rename		= jk_rename,
    .link		= jk_link,
	.chmod		= jk_chmod,
	.chown		= jk_chown,
	.truncate	= jk_truncate ,
	// .utime       = jk_utime,
	.open		= jk_open,
	.read		= jk_read,
	.write		= jk_write,
	.statfs		= jk_statfs,
	// .flush		= jk_flush,
	.release	= jk_release,
	// .fsync		= jk_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= jk_setxattr,
	.getxattr	= jk_getxattr,
	.listxattr	= jk_listxattr,
 	.removexattr= jk_removexattr,
#endif
	.opendir	= jk_opendir,
	.readdir	= jk_readdir,
	.releasedir	= jk_releasedir,
	// .fsyncdir	= jk_fsyncdir,
	// .init		= jk_init,
	// .destroy	= jk_destroy,
	.access		= jk_access,
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
    if ((fp = fopen("sjk_args", "r")) == NULL) {
        perror("sjk_args");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%zu %s %s", &THRESH, HDDPATH, MP);
    return FJK_SUCCESS;
}

int main(int argc, char* argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    
    // read arguments, mainly ssdpath, hddpath, and mountpoint.
    if (argc < 4) {
        read_args_from_file();
        if (argc == 2 && strncmp(argv[1], "-d", 2) == 0) 
            fuse_opt_add_arg(&args, "-d");
    } else if (argc != 5) {
        fputs("error in threshold, ssdpath, hddpath, mountpoint\n", stderr);
        exit(EXIT_FAILURE);
    } else {
        THRESH = atoi(argv[1]);
        strcpy(HDDPATH, argv[2]);
        strcpy(MP, argv[3]);
    }

    fuse_opt_add_arg(&args, MP);
    return fuse_main(args.argc, args.argv, &jk_ops, NULL);
}


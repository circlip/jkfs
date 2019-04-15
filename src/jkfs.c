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
#define JK_SUCCESS 0
#define MAXPATH 256
static char SSDPATH[MAXPATH];
static char HDDPATH[MAXPATH];
static char MP[MAXPATH];
static size_t THRESH;
static unsigned long count = 0;


#define path2ssdpath    strcpy(ssdpath, SSDPATH);   \
                        strcat(ssdpath, path)

#define ssdpath2xattrpath   strcpy(xattrpath, ssdpath); \
                            strcpy(strrchr(xattrpath, '/') + 1, ".xattr_"); \
                            strcat(xattrpath, strrchr(path, '/') + 1)

static int xattr_exist(const char* ssdpath) {
    int res;
    char xattrpath[MAXPATH];
    ssdpath2xattrpath;
    struct stat stbuf;
    res = lstat(xattrpath, &stbuf);
    if (res == 0)
        return 1;
    else 
        return 0;
}

static int path2ssdpath4links(const char* path, char* ssdpath) {
    char link[MAXPATH];
    int res;
    path2ssdpath;
    if (xattr_exist(ssdpath)) {
        return 1;
    }
    res = readlink(ssdpath, link, MAXPATH - 1);
    while (1) {
        if (res == -1) {
            return -errno;
        }
        link[res] = 0;
        strcpy(strrchr(ssdpath, '/') + 1, link);
        if (xattr_exist(ssdpath)) {
            return 1;
        }
        res = readlink(ssdpath, link, MAXPATH - 1);
    }
}

static int ssdpath2hddpath(const char* ssdpath, char* hddpath) {
    struct stat stbuf;
    char target[MAXPATH];
    int res;
    res = lstat(ssdpath, &stbuf);
    if (res == -1) {
        return -errno;
    }
    assert(S_ISLINK(stbuf.st_mode));
    res = readlink(ssdpath, target, MAXPATH - 1);
    if (res == -1) {
        return -errno;
    }
    target[res] = 0;
    strcpy(hddpath, HDDPATH);
    strcat(hddpath, target);
    return JK_SUCCESS;
}

static int jk_create(const char *path, mode_t mode, struct fuse_file_info *info) {
    int fd;
    char ssdpath[MAXPATH];
    path2ssdpath;
    fd = create(ssdpath, mode);
    if (fd == -1)
        return -errno;
    close(fd);
    return JK_SUCCESS;
}

static int jk_getattr(const char *path, struct stat *statbuf) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath;
    res = lstat(ssdpath, stbuf);
    if (res == -1)
        return -errno;
    if (S_ISLINK(statbuf->st_mode) && xattr_exist(ssdpath)) {
        int fd;
        // fixme: call ssdpath2xattrpath twice!
        ssdpath2xattrpath;
        fd = open(xattrpath, O_RDONLY);
        if (fd != -1) {
            res = read(fd, statbuf, sizeof(*statbuf));
            close(fd);
            if (res != sizeof(*statbuf)) {
                return -errno;
            } else {
                return JK_SUCCESS;
            }
        }
    }
    return 0;
}

static int jk_access(const char *path, int mask) {
    int res;
    char ssdpath[MAXPATH];
    res = path2ssdpath4links(path, ssdpath);
    char _path[MAXPATH];
    if (xattr_exist(ssdpath)) {
        ssdpath2hddpath(ssdpath, _path);
    } else {
        strcpy(_path, ssdpath);
    }

    res = access(_path, mask);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_readlink(const char *path, char* buf, size_t size) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath;
    if (xattr_exist(ssdpath)) {
        errno = EINVAL;
        return -errno;
    }
    res = readlink(ssdpath, buf, size - 1);
    if (res == -1) {
        return -errno;
    }
    buf[res] = 0;
    return JK_SUCCESS;
}

static int jk_readdir(const char *path, char *buf, 
                      fuse_fill_dir_t filler, 
                      off_t offset, struct fuse_file_info *info) {
    DIR *dp;
    struct dirent *de;
    (void)offset;
    (void)info;
    char ssdpath[MAXPATH];
    path2ssdpath;
    dp = opendir(ssdpath);
    if (dp == NULL) {
        return -errno;
    }
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        if (strncmp(de->d_name, ".xattr_", 7) == 0) {
            continue;
        }
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
        perror("args_file");
        exit(EXIT_FAILURE);
    }
    fscanf(fd, "%zu %s %s %s", &THRESH, SSDPATH, HDDPATH, MP);
    return;
}

int main()
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
        THRES = atoi(argv[1]);
        strcpy(SSDPATH, argv[2]);
        strcpy(HDDPATH, argv[3]);
        strcpy(MP, argv[4]);
    }

    fuse_opt_add_arg(&args, MP);
    return fuses_main(args.argc, args.argv, &jk_ops, NULL);
}











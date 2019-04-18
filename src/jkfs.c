#define FUSE_USE_VERSION 29 

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
                            strcat(xattrpath, strrchr(ssdpath, '/') + 1)


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
    assert(S_ISLNK(stbuf.st_mode));
    res = readlink(ssdpath, target, MAXPATH - 1);
    if (res == -1) {
        return -errno;
    }
    target[res] = 0;
    strcpy(hddpath, HDDPATH);
    strcat(hddpath, target);
    return JK_SUCCESS;
}

static int jk_creat(const char *path, mode_t mode, struct fuse_file_info *info) {
    int fd;
    char ssdpath[MAXPATH];
    path2ssdpath;
    fd = creat(ssdpath, mode);
    if (fd == -1)
        return -errno;
    close(fd);
    return JK_SUCCESS;
}

static int jk_getattr(const char *path, struct stat *stbuf) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath;
    res = lstat(ssdpath, stbuf);
    if (res == -1)
        return -errno;
    char xattrpath[MAXPATH];
//    struct stat stbuf;
    ssdpath2xattrpath;
    res = lstat(xattrpath, &stbuf);

    if (S_ISLNK(statbuf->st_mode) && !res) {
        int fd;
        fd = open(xattrpath, O_RDONLY);
        if (fd != -1) {
            res = read(fd, stbuf, sizeof(*stbuf));
            close(fd);
            if (res != sizeof(*stbuf)) {
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
    char ssdpath[MAXPATH];
    path2ssdpath;
    int res;

    if (S_ISREG(mode)) {
        res = open(ssdpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0) {
            res = close(res);
        }
    } else if (S_ISFIFO(mode)) {
        res =  mkfifo(ssdpath, mode);
    } else {
        res = mknode(ssdpath, mode, rdev);
    }

    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_mkdir(const char *path, mode_t mode) {
    char ssdpath[MAXPATH];
    path2ssdpath;
    int res;
    res = mkdir(ssdpath, mode);
    if (res == -1) {
        return -errno;
    }
    return  JK_SUCCESS;
}

static int jk_unlink(const char *path) {
    char ssdpath[MAXPATH];
    path2ssdpath;
    int res;
    char xattrpath[MAXPATH];
    struct stat stbuf;
    ssdpath2xattrpath;
    res = lstat(xattrpath, &stbuf);

    if (res == 0) {
        char hddpath[MAXPATH];
        res = ssdpath2hddpath(ssdpath, hddpath);
        if (res) {
            return -errno;
        }
        res = unlink(hddpath);
        res = unlink(xattrpath);
    }
    res = unlink(ssdpath);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_rmdir(const char *path) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath;
    res = rmdir(ssdpath);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_symlink(const char *from, const char *to) {
    int res;
    char *path = to;
    char ssdpath[MAXPATH];
    path2ssdpath;
    res = symlink(from, ssdpath);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_rename(const char *from, const char *to) {
    int res;
    char* path = from;
    char ssdpath[MAXPATH];
    char ssdpath_from[MAXPATH];
    char ssdpath_to[MAXPATH];
    path2ssdpath;
    strcpy(ssdpath_from, ssdpath);
    path = to;
    path2ssdpath;
    strcpy(ssdpath_to, ssdpath);
    res = rename(ssdpath_from, ssdpath_to);
    if (res == -1) {
        return -errno;
    }
    
    char xattrpath[MAXPATH];
    strcpy(ssdpath, ssdpath_from);
    ssdpath2xattrpath;
    struct stat stbuf;
    res = lstat(xattrpath, &stbuf);
    if (res == 0) {
        char xattrpath_from[MAXPATH];
        char xattrpath_to[MAXPATH];
        strcpy(xattrpath_from, xattrpath);

        strcpy(ssdpath, ssdpath_to);
        ssdpath2xattrpath;
        strcpy(xattrpath_to, xattrpath);
        res = rename(xattrpath_from, xattrpath_to);
        if (res == -1) {
            return -errno;
        }
    }
    return JK_SUCCESS;
}

static int jk_chmod(const char *path, mode_t mode) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath4links(path, ssdpath);
    char xattrpath[MAXPATH], hddpath[MAXPATH];
    ssdpath2xattrpath;
    struct stat stbuf;
    res = lstat(xattrpath, &stbuf);
    if (res == 0) {
        ssdpath2hddpath(ssdpath, hddpath);
        if (chmod(hddpath, mode) == -1) {
            return -errno;
        }
        if (lstat(hddpath, &stbuf) != 0) {
            return -errno;
        }
        int fd;
        if ((fd = open(xattrpath, O_WRONLY)) < 0) {
            return -errno;
        }
        if ((res = write(xattrpath, &stbuf, sizeof(stbuf))) != sizeof(stbuf)) {
            close(fd);
            return -errno;
        }
        close(fd);
        return JK_SUCCESS;
    }
}

static int jk_chown(const char* path, uid_t userid, gid_t groupid) {
    int res;
    char ssdpath[MAXPATH];
    path2ssdpath;
    res = lchown(ssdpath, userid, groupid);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_truncate(const char *path, off_t size) {
    char ssdpath[MAXPATH], _path[MAXPATH], xattrpath[MAXPATH], hddpath[MAXPATH];
    int res;
    path2ssdpath4links;
    ssdpath2xattrpath;
    struct stat stbuf;
    res = lstat(xattrpath, &stbuf);
    // size larger than threshold and should be placed in hdd
    if (size > THRESH) {
        if (res != 0) {     // original placed in ssd
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ssdpath2hddpath(ssdpath, hddpath);
            // fixme: why the file should be named in this way?
            sprintf(strrchr(hddpath, '/') + 1, "%s_%u%lu", strrchr(path, '/') + 1, 
                        (unsigned int)tv.tv_sec, __sync_fetch_and_add(&count, 1));
            rename(ssdpath, hddpath);
            res = truncate(hddpath, size);
            if (res) {
                return -errno;
            }
            unlink(ssdpath);
            symlink(strrchr(hddpath, '/') + 1, ssdpath);

        } else {            // originally placed in hdd
            ssdpath2hddpath(ssdpath, hddpath);
            res = truncate(hddpath, size);
            if (res) {
                return -errno;
            }
        }
    } else {    // size smaller than threshold 
        if (res != 0) {
            truncate(ssdpath, size);
        } else {
            // move from hdd to ssd
            ssdpath2hddpath(ssdpath, hddpath);
            truncate(hddpath, size);
            unlink(ssdpath);
            rename(hddpath, ssdpath);
            unlink(hddpath);
            unlink(xattrpath);
        }
    }
    return JK_SUCCESS;
}

static int jk_utimens(const char *path, const struct timespec ts[2]){
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    path2ssdpath;
    ssdpath2xattrpath;
    struct stat stbuf;
    res = lstat(xattrpath, &stbuf);
    if (res == 0) {
        int fd;
        fd = open(xattr, O_RDWR);
        if (fd < 0) {
            return -errno;
        }
        res = read(fd, &stbuf, sizeof(stbuf));

        // begin error handling
        if (res != sizeof(stbuf)) {
            close(fd);
            return -errno;
        }
        // end of error handling

        memcpy(&stbuf.st_atime, ts, sizeof(*ts));
        memcpy(&stbuf.st_mtime, &(ts[1]), sizeof(*ts));
        res = write(fd, &stbuf, sizeof(stbuf));

        // begin error handling
        if (res == -1) {
            close(fd);
            return -errno;
        }
        // end of error handling

        close(fd);
        return JK_SUCCESS;
    } else {
        res = utimensat(0, ssdpath, ts, AT_SYMLINK_NOFOLLOW);
        if (res == -1) {
            return -errno;
        }
        return JK_SUCCESS;
    }
}

static int jk_open(const char *path, struct fuse_file_info *info) {
    int res;
    char _path[MAXPATH], xattrpath[MAXPATH];
    path2ssdpath;
    ssdpath2xattrpath;
    struct stat stbuf;
    res =  lstat(xattrpath, &stbuf);
    if (res == 0) {
        ssdpath2hddpath(ssdpath, _path);
    } else {
        strcpy(_path, ssdpath);
    }

    res = open(_path, info->flags);
    if (res == -1) {
        return -errno;
    }
    return res;

}

// fixme: file descriptor instead of path is passed to function write.
// this implementation seems to be wrong!
static int jk_read(const char *path, char *buf, 
                   size_t size, off_t offset, 
                   struct fuse_file_info *info) {
    int fd, res;
    fd = jk_open(path, info);
    if (fd < 0) {
        return -errno;
    } 
    res = read(fd, buf, size, offset);
    if (res == -1) {
        return -errno;
    }
    close(fd);
    return res;
}

// fixme: file descriptor instead of path is passed to function write.
// this implementation seems to be wrong!
static int jk_write(const char *path, const char *buf, 
                    size_t size, off_t offset, 
                    struct fuse_file_info *info) {
    int fd, res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH], hddpath[MAXPATH];
    char _path[MAXPATH];
    struct stat stbuf;
    path2ssdpath;
    ssdpath2xattrpath;
    res = lstat(xattrpath, &stbuf);
    if (res == 0) {
           // xattrpath exists, originally placed in hdd
           ssdpath2hddpath(ssdpath, hddpath);
           flag = 1; // xattr file needs to be edited

    } else {
        // xattrpath does not exist, originally placed in ssd 
        
        res = stat(ssdpath, &stbuf);
        if (res == -1) {
            return -errno;
        }

        if (stbuf.st_size + size > THRESH + THRESH >> 1) {
            // too larger, move to hdd
            // ssdpath2hddpath(ssdpath, hddpath);
            flag = 1; // need to create an .xattr file

            // fixme: why should the file should be name in this way
            struct timeval tv;
            gettimeofday(&tv, NULL);
            sprintf(hddpath, "%s%s_%u%lu", HDDPATH, strrchr(path, '/') + 1, 
                    (unsigned int)tv.tv_sec, __sync_fetch_and_add(&count, 1));            

            rename(ssdpath, hddpath);
            unlink(ssdpath);
            symlink(strrchr(hddpath, '/') + 1, ssdpath);
            strcpy(_path, hddpath);
        } else {
            // no need to move
            strcpy(_path, ssdpath);
        }
    }

    fd = open(_path, O_WRONLY);
    res = pwrite(fd, buf, size, offset);
    if (res != size) {
        return -errno;
    }
    close(fd);

    // update xattr

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
    fd = jk_open(path, O_WRONLY);
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
    fd = jk_open(path, O_WRONLY);
    if (fd < 0) {
        return -errno;
    }
    res = fallocate(path, mode, offset, length);
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
	// .opendir	= jk_opendir,
	.readdir	= jk_readdir,
	// .releasedir	= jk_releasedir,
	// .fsyncdir	= jk_fsyncdir,
	// .init		= jk_init,
	// .destroy	= jk_destroy,
	.creat		= jk_creat,
	// .ftruncate	= jk_ftruncate,
	.fgetattr	= jk_fgetattr,
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
        THRESH = atoi(argv[1]);
        strcpy(SSDPATH, argv[2]);
        strcpy(HDDPATH, argv[3]);
        strcpy(MP, argv[4]);
    }

    fuse_opt_add_arg(&args, MP);
    return fuses_main(args.argc, args.argv, &jk_ops, NULL);
}


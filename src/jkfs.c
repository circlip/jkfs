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

// todo: some need deep and some does not. REVIEW ALL !!!
static int path2ssd_deep(const char *path, char *ssdpath) {
    strcpy(ssdpath, SSDPATH);
    strcat(ssdpath, path);
    char xattrpath[MAXPATH];
    int res;
    char linkedname[MAXPATH];
    // if ssdpath is a symlink, than parse the link
    // todo: how to create a link to ssd elegantly
    while ((res = readlink(ssdpath, linkedname, MAXPATH - 1)) != -1 &&
            ssd2xattr(ssdpath, xattrpath) == -1 ) {
        linkedname[res] = 0;
        strcpy(strrchr(ssdpath, '/') + 1, linkedname);
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
    res = readlink(ssdpath, target, MAXPATH - 1);
    if (res == -1) {
        return -errno;
    }
    target[res] = 0;
    sprintf(hddpath, "%s/%s", HDDPATH, target);
    return JK_SUCCESS;    
}

static int jk_creat(const char *path, mode_t mode, struct fuse_file_info *info) {
    char ssdpath[MAXPATH];
    int res;
    res = path2ssd(path, ssdpath);
    res = creat(ssdpath, mode);
    if (res == -1) {
        return -errno;
    }
    close(res);
    return JK_SUCCESS;
}

static int jk_getattr(const char *path, struct stat *stbuf) {
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    int res;
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);

    /**
    * if xattr file does not exist: 
    * the file is located in ssd and lstat it.
    * if xattr file exist, which means that the file is located in hdd,
    * and its information is written in the xattr file
    */
    if (res < 0) {
        res = lstat(ssdpath, stbuf);
        if (res == -1) {
            return -errno;
        }
        return JK_SUCCESS;
    } else {
        int fd = open(xattrpath, O_RDONLY);
        if (fd < 0) {
            return -errno;
        }
        res = read(fd, stbuf, sizeof(*stbuf));
        if (res != sizeof(*stbuf)) {
            close(fd);
            return -errno;
        }
        close(fd);
        return JK_SUCCESS;
    }
}

static int jk_access(const char *path, int mask) {
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    int res;
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == -1) {
        res = access(ssdpath, mask);
        return res;
    } else {
        // sad. it seems better to read the xattr file to find out.
        // but somehow it is troublesome
        res = access(hddpath, mask);
        return res;
    }
}

/**
* read symlink only, regardless of xattr file
*/
static int jk_readlink(const char *path, char* buf, size_t size) {
    char ssdpath[MAXPATH];
    sprintf(ssdpath, "%s%s", SSDPATH, path);
    int res;
    res = readlink(ssdpath, buf, size - 1);
    if (res == -1) {
        return -errno;
    }
    buf[res] = 0;
    return JK_SUCCESS;
}

static int jk_opendir(const char *path, struct fuse_file_info *info) {
	char ssdpath[MAXPATH];
    path2ssd(path, ssdpath);
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
    char ssdpath[MAXPATH];
    path2ssd(path, ssdpath);
    int res;
    if (S_ISREG(mode)) {
        res = open(ssdpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0) {
            close(res);
        }
    } else if (S_ISFIFO(mode)) {
        res = mkfifo(ssdpath, mode);
    } else {
        res = mknod(ssdpath, mode, rdev);
    }
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_mkdir(const char *path, mode_t mode) {
    // easy. all happen in ssd
    char ssdpath[MAXPATH];
    int res;
    sprintf(ssdpath, "%s%s", SSDPATH, path);
    res = mkdir(ssdpath, mode);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

/**
* this remove a file
*/
static int jk_unlink(const char *path) {
    // to ensure concurrency, 
    // unlink hddpath first(if exists), and ssdpath lastly
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    // if xattr exists
    if (res == 0) {
        char hddpath[MAXPATH];
        res = xattr2hdd(xattrpath, hddpath);
        if (res != 0) {
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
    // easy. all happen in ssdpath
    char ssdpath[MAXPATH];
    int res;
    res = path2ssd(path, ssdpath);
    res = rmdir(ssdpath);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

// the path is where the link points to
// which is in accordance with the system call
static int jk_symlink(const char *path, const char *link) {
    int res;
    char ssdlink[MAXPATH];
    res = path2ssd(link, ssdlink);
    res = symlink(path, ssdlink);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_rename(const char *path, const char *newpath) {
    int res;
    char ssdpath[MAXPATH], ssdnewpath[MAXPATH];
    char xattrpath[MAXPATH], xattrnewpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = path2ssd(newpath, ssdnewpath);
    if ((res = ssd2xattr(ssdpath, xattrpath)) == 0) {
        res = ssd2xattr(ssdnewpath, xattrnewpath);
        res = rename(xattrpath, xattrnewpath);
        if (res == -1) {
            return -errno;
        }
        // no need to rename hddpath because it is always linked to.
    }
    res = rename(ssdpath, ssdnewpath);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;
}

static int jk_chmod(const char *path, mode_t mode) {
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        if ((res = chmode(xattrpath, mode)) == -1) {
            return -errno;
        }
        char hddpath[MAXPATH];
        if ((res = xattr2hdd(xattrpath, hddpath)) == -1) {
            return -errno;
        }
        if ((res = chmod(hddpath, mode)) == -1) {
            return -errno;
        }
        struct stat stbuf;
        lstat(hddpath, &stbuf);
        int fd;
        if ((fd = open(xattrpath, O_WRONLY))) < 0) {
            return -errno;
        }
        if ((res = write(fd, &stbuf, sizeof(stbuf)))!= sizeof(stbuf)) {
            return -errno;
        }
    }
    res = chmod(ssdpath, mode);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;  
}

static int jk_chown(const char* path, uid_t uid, gid_t gid) {
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        if ((res = chown(xattrpath, uid, gid)) == -1) {
            return -errno;
        }
        char hddpath[MAXPATH];
        if ((res = xattr2hdd(xattrpath, hddpath)) == -1) {
            return -errno;
        }
        if ((res = chown(hddpath, uid, gid)) == -1) {
            return -errno;
        }
        struct stat stbuf;
        lstat(hddpath, &stbuf);
        int fd;
        if ((fd = open(xattrpath, O_WRONLY))) < 0) {
            return -errno;
        }
        if ((res = write(fd, &stbuf, sizeof(stbuf)))!= sizeof(stbuf)) {
            return -errno;
        }
    }
    res = chown(ssdpath, uid, gid);
    if (res == -1) {
        return -errno;
    }
    return JK_SUCCESS;  
} 

static int jk_truncate(const char *path, off_t size) {
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd_deep(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        // originally located in hdd
        char hddpath[MAXPATH];
        res = xattr2hdd(xattrpath, hddpath);
        if (size < THRESH) {
            // move to ssd
            res = truncate(hddpath, size);
            unlink(ssdpath);
            res = rename(hddpath, ssdpath);
            unlink(hddpath);
            unlink(xattrpath);
            return JK_SUCCESS;
        } else {
            // remain in hdd
            res = truncate(hddpath, size);
            // update xattr
            struct stat st;
            if ((res = lstat(hddpath, &st)) == -1) {
                return -errno;
            }
            int fd;
            if ((fd = open(xattrpath, O_WRONLY)) < 0) {
                return -errno;
            }
            if ((res = write(fd, &st, sizeof(st))) != sizeof(st)) {
                close(fd);
                return -errno;
            }
            close(fd);
            return JK_SUCCESS;
        }
    } else {
        // origianlly in ssd
        if (size > THRESH + (THRESH >> 1)) {
            // move to hdd
            char hddpath[MAXPATH];
            struct timesval timestamp;
            gettimeofday(&timestamp, NULL);
            // todo: seems no need to name it this way
            spritnf(hddpath, "%s/%s_%u%lu", HDDPATH, strrchr(path, '/') + 1),
                        (unsigned int)timestamp.tv_sec,
                        __sync_fetch_and_add(&count, 1));
            res = rename(ssdpath, hddpath);
            res = truncate(hddpath, size);
            res = unlink(ssdpath);
            res = symlink(strrchr(hddpath, '/') + 1, ssdpath);
            // update xattr
            struct stat st;
            if ((res = lstat(hddpath, &st)) == -1) {
                return -errno;
            }
            int fd;
            if ((fd = open(xattrpath, O_WRONLY | O_CREAT)) < 0) {
                return -errno;
            }
            if ((res = write(fd, &st, sizeof(st))) != sizeof(st)) {
                close(fd);
                return -errno;
            }
            close(fd);
            return JK_SUCCESS;
        } else {
            // remains in ssd
            res = truncate(ssdpath, size);
            return JK_SUCCESS;
        }
    }

}

static int jk_utimens(const char *path, const struct timespec ts[2]){
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        struct stat stbuf;
        int fd;
        if ((fd = open(xattrpath, O_RDWR)) < 0) {
            return -errno;
        }
        if ((res = read(fd, &stbuf, sizeof(stbuf))) != sizeof(stbuf)) {
            return -errno;
        }
        memcpy(&(stbuf.st_atime), ts, sizeof(*ts));
        memcpy(&(stbuf.st_mtime), ts + 1, sizeof(*ts));
        if ((res = write(fd, &stbuf, sizeof(stbuf))) != sizeof(stbuf)) {
            close(fd);
            return -errno;
        }
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

static int jk_open(const char *path, struct fuse_file_info *fi) {
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        char hddpath[MAXPATH];
        res = xattr2hdd(xattrpath, hddpath);
        int fd = open(hddpath, fi->flags);
        if (fd < 0) {
            return -errno;
        }
        fi->fh = fd;
        return fd;
    } else {
        int fd = open(ssdpath, fi->flags);
        if (fd < 0) {
            return -errno;
        }
        fi->fh = fd;
        return fd;
    }
}

static int jk_read(const char *path, char *buf, 
                   size_t size, off_t offset, 
                   struct fuse_file_info *fi) {
    int res;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1) {
        return -errno;
    }
    return res;
}

static int jk_write(const char *path, const char *buf, 
                    size_t size, off_t offset, 
                    struct fuse_file_info *fi) {
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res != 0) {
        // res != 0 means that xattr does not exist,
        // file is located in ssd
        if (offset + size > THRESH + (THRESH >> 1)) {
            // should be move to hdd

            // // write to ssd first
            // res = pwrite(fi->fh, buf, size, offset);
            close(fi->fh);

            // move to hdd, unlink original, create xattr, open hdd file to assign new fi->fh
            char hddpath[MAXPATH];
            struct timesval timestamp;
            gettimeofday(&timestamp, NULL);
            // todo: seems no need to name it this way
            spritnf(hddpath, "%s/%s_%u%lu", HDDPATH, strrchr(path, '/') + 1),
                        (unsigned int)timestamp.tv_sec,
                        __sync_fetch_and_add(&count, 1));
            rename(ssdpath, hddpath);
            unlink(ssdpath);
            symlink(strrchr(hddpath, '/') + 1, ssdpath);
            int fd = open(hddpath, O_WRONLY);
            res = pwrite(fd, buf, size, offset);
            fi->fh = fd;

            int ress, fdd;
            struct stat st;
            if ((ress = lstat(hddpath, &st)) == -1) {
                return -errno;
            }
            if ((fdd = open(xattrpath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
                return -errno;
            }
            if ((ress = write(fdd, &st, sizeof(st))) != sizeof(st)) {
                close(fdd);
                return -errno;
            }
            close(fdd);
            return res;
        } else {
            // remain in ssd
            res = pwrite(fi->fh, buf, size, offset);
            return res;
        }
    } else {
        // file is located in hdd
        res = pwrite(fi->fh, buf, size, offset);
        struct stat st;
        int ress, fdd;
        if ((ress = lstat(hddpath, &st)) == -1) {
            return -errno;
        } 
        if ((fdd = open(xattrpath, O_WRONLY)) < 0) {
            return -errno;
        }
        if ((ress = write(fdd, &st, sizeof(st))) != sizeof(st)) {
            close(fdd);
            return -errno;
        }
        close(fdd);
        return res;
    }
}

static int jk_statfs(const char *path, struct statvfs *statbuf) {
    fprintf(stderr, "Not implemented yet.\n");
    return -1;
}

static int jk_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);
    return JK_SUCCESS;
}

static int jk_fsync(const char *path, int isdatasync,
                    struct fuse_file_info *fi) {
    // todo:
    (void) path;
    (void) fi;
    return JK_SUCCESS;
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
                        struct fuse_file_info *fi) {
    int fd, res;
    (void) fi;
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


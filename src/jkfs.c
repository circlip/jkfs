#define FUSE_USE_VERSION 29 
#define _GNU_SOURCE 500
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
#define BUF_SIZE 131072
static char SSDPATH[MAXPATH];
static char HDDPATH[MAXPATH];
static char MP[MAXPATH];
static size_t THRESH;
static int realfd[128];
static unsigned long count = 0;

//#define debug 11

#ifdef debug
#define start {dfp = fopen(debugpath, "a+"); fprintf(dfp, "\n### %s: %s \n", name, path);}
#define end {fprintf(dfp, "### end of %s\n", name); fclose(dfp);}
static char debugpath[MAXPATH];
static char name[32];
static FILE *dfp;
static int dres;
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
//    if (!S_ISLNK(st.st_mode)) {
//        return -1;
//    }
    return JK_SUCCESS;
}

// todo: some need deep and some does not. REVIEW ALL !!!
static int path2ssd_deep(const char *path, char *ssdpath) {
    strcpy(ssdpath, SSDPATH);
    strcat(ssdpath, path);
    char xattrpath[MAXPATH];
    int res;
    char linkedname[MAXPATH];
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
static int ssd2hdd(const char* ssdpath, char* hddpath) {
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
		if (res == 0) {
			char hddpath[MAXPATH];
			res = ssd2hdd(ssdpath, hddpath);
			res = lstat(hddpath, stbuf);
			if (res != 0) 
				return -errno;
		} else if (res != sizeof(*stbuf)) {
            close(fd);
            return -errno;
        }
        close(fd);
        return JK_SUCCESS;
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
        res = ssd2hdd(ssdpath, hddpath);
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
        if ((res = chmod(xattrpath, mode)) == -1) {
            return -errno;
        }
        char hddpath[MAXPATH];
        if ((res = ssd2hdd(ssdpath, hddpath)) == -1) {
            return -errno;
        }
        if ((res = chmod(hddpath, mode)) == -1) {
            return -errno;
        }
        struct stat stbuf;
        lstat(hddpath, &stbuf);
        int fd;
        if ((fd = open(xattrpath, O_WRONLY)) < 0) {
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
        if ((res = ssd2hdd(ssdpath, hddpath)) == -1) {
            return -errno;
        }
        if ((res = chown(hddpath, uid, gid)) == -1) {
            return -errno;
        }
        struct stat stbuf;
        lstat(hddpath, &stbuf);
        int fd;
        if ((fd = open(xattrpath, O_WRONLY)) < 0) {
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
        res = ssd2hdd(ssdpath, hddpath);
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
            struct timeval timestamp;
            gettimeofday(&timestamp, NULL);
            sprintf(hddpath, "%s/%s_%u%lu", HDDPATH,
						strrchr(path, '/') + 1,
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

static int jk_open(const char *path, struct fuse_file_info *fi) {
#ifdef debug
	strcpy(name, "jk_open");
	start
#endif
    int res, fd;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
		// file located in hdd
        char hddpath[MAXPATH];
        res = ssd2hdd(ssdpath, hddpath);
        fd = open(hddpath, fi->flags);
        if (fd < 0) {
            return -errno;
        }
    } else {
		// file located in ssd
        fd = open(ssdpath, fi->flags);
        if (fd < 0) {
            return -errno;
        }
    }
	fi->fh = fd;
#ifdef debug
	fprintf(dfp, "fd is %d\n", (int)fi->fh);
	end
#endif
	return JK_SUCCESS;
}

static int jk_read(const char *path, char *buf, 
                   size_t size, off_t offset, 
                   struct fuse_file_info *fi) {
    int res, fd;
	char ssdpath[MAXPATH], xattrpath[MAXPATH];
	res = path2ssd(path, ssdpath);
	res = ssd2xattr(ssdpath, xattrpath);
	fd = (int)fi->fh;
// 	if (res == 0 && realfd[fd] >= 0) {
// 		fd = realfd[fd];
// 	}
	res = pread(fd, buf, size, offset);
	if (res < 0) {
		return -errno;
	}
	return res;
}

static int jk_write(const char *path, const char *buf, 
					size_t size, off_t offset,
					struct fuse_file_info *fi) {
#ifdef debug
	strcpy(name, "jk_write");
	start
	fprintf(dfp, "fd is %d\n", (int)fi->fh);
#endif
	int res, fd;
	char ssdpath[MAXPATH], xattrpath[MAXPATH], hddpath[MAXPATH];
	res = path2ssd(path, ssdpath);
	res = ssd2xattr(ssdpath, xattrpath);
	if (res != 0) {
 		if (offset + size > THRESH) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			sprintf(hddpath, "%s/%s_%u%lu", HDDPATH, strrchr(path, '/') + 1,
					(unsigned int)tv.tv_sec, __sync_fetch_and_add(&count, 1));
            // copy ssd file to hdd
			int fdd;
			char cur_buf[THRESH];
			fdd = open(hddpath, O_WRONLY | O_CREAT, 0644);
			fd = open(ssdpath, O_RDONLY);
			res = read(fd, cur_buf, THRESH);
			res = write(fdd, cur_buf, res);
			close(fd);
            
            // handling file descriptor
			fd = (int)fi->fh;
			realfd[fd] = fdd;
            // create xattr file
			fd = open(xattrpath, O_WRONLY | O_CREAT | O_EXCL, 0644);
			close(fd);
            // unlink and create symlink
			unlink(ssdpath);
			symlink(strrchr(hddpath, '/') + 1, ssdpath);
            // prepared to write in hdd file
			fd = fdd;
		} else {
#ifdef debug
			fprintf(dfp, "%s: file not large, remains in ssd\n", path);
#endif
			fd = (int)fi->fh;
		}
	} else {
#ifdef debug
		fprintf(dfp, "%s: xattr exists, already located in hdd\n", xattrpath);
#endif
		fd = (int)fi->fh;
		int fdd = realfd[fd];
		if (fdd == -1) {
			ssd2hdd(ssdpath, hddpath);
			fdd = open(hddpath, O_WRONLY);
			realfd[fd] = fdd;
			fd = fdd;
		} else {
			fd = realfd[fd];
		}
	}
#ifdef debug
	fprintf(dfp, "current fd is %d\n", fd);
#endif
	res = pwrite(fd, buf, size, offset);
	if (res < 0) {
		return -errno;
	}
#ifdef debug
//	fprintf(dfp, "%d bytes written, current fd is %d\n", res, fd);
	end
#endif
	return res;
}

static int jk_statfs(const char *path, struct statvfs *statbuf) {
    fprintf(stderr, "Not implemented yet.\n");
    return -1;
}

static int jk_release(const char *path, struct fuse_file_info *fi) {
#ifdef debug
	strcpy(name, "jk_release");
	start
	fprintf(dfp, "current fd is %d \n", (int)fi->fh);
#endif
	char ssdpath[MAXPATH], xattrpath[MAXPATH];
	int res;
	res = path2ssd(path, ssdpath);
	res = ssd2xattr(ssdpath, xattrpath);
	if (res == 0) {
		// file located in hdd, should update xattr
		char hddpath[MAXPATH];
		struct stat st;
		res = ssd2hdd(ssdpath, hddpath);
		res = lstat(hddpath, &st);
		int fd = open(xattrpath, O_WRONLY | O_CREAT);
		res = write(fd, &st, sizeof(st));
		close(fd);
		// end of xattr file 
		// close ssdpath and hddpath files
		fd = (int)fi->fh;
		close(fd);
#ifdef debug
		fprintf(dfp, "ssd file %d closed ", fd);
#endif
		fd = realfd[fd];
		close(fd);
#ifdef debug
		fprintf(dfp, " and hdd file %d closed \n", fd);
#endif
		fd = (int)fi->fh;
		realfd[fd] = -1;
	} else {
		// file located in ssd, need to do nothing
		close((int)fi->fh);
	}
#ifdef debug
	end
#endif
    return JK_SUCCESS;
}

static int jk_fsync(const char *path, int isdatasync,
                    struct fuse_file_info *fi) {
    // todo:
    (void) path;
    (void) fi;
    return JK_SUCCESS;
}



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

static int jk_opendir(const char *path, struct fuse_file_info *info) {
	char ssdpath[MAXPATH];
    path2ssd(path, ssdpath);
	info->fh = (intptr_t)opendir(ssdpath);
	return JK_SUCCESS;
}

static int jk_readdir(const char *path, void *buf, 
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

static int jk_access(const char *path, int mask) {
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    int res;
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == -1) {
        res = access(ssdpath, mask);
        return res;
    } else {
        // fixme: stupid access: it seems better to read the xattr file to find out.
        // but somehow it is troublesome
        // obtain from exact file is simple but costly
        char hddpath[MAXPATH];
        res = ssd2hdd(ssdpath, hddpath);
        res = access(hddpath, mask);
        return res;
    }
}

static int jk_creat(const char *path, mode_t mode, struct fuse_file_info *fi) {
#ifdef debug
	strcpy(name, "jk_creat");
	start
#endif
    char ssdpath[MAXPATH];
    int res, fd;
    res = path2ssd(path, ssdpath);
    fd = creat(ssdpath, mode);
	// fd = open(ssdpath, O_CREAT | O_WRONLY, mode);
#ifdef debug
	fprintf(dfp, "%s created in ssd, and the file descriptor is %d\n", ssdpath, fd);
#endif
    if (fd < 0) {
        return -errno;
    }
	fi->fh = fd;
#ifdef debug
	end
#endif
    return JK_SUCCESS;
}

static int jk_utimens(const char *path, const struct timespec ts[2]){
#ifdef debug
	strcpy(name, "jk_utimens");
	start
#endif
    int res;
    char ssdpath[MAXPATH], xattrpath[MAXPATH];
    res = path2ssd(path, ssdpath);
    res = ssd2xattr(ssdpath, xattrpath);
    if (res == 0) {
        struct stat stbuf;
        int fd;
		fd = open(xattrpath, O_RDWR);
        // if ((fd = open(xattrpath, O_RDWR | O_CREAT)) < 0) {
        //     return -errno;
        // }
		res = read(fd, &stbuf, sizeof(stbuf));
		if (res == 0) {
			char hddpath[MAXPATH];
			res = ssd2hdd(ssdpath, hddpath);
			res = utimensat(0, hddpath, ts, AT_SYMLINK_NOFOLLOW);
			if (res == -1) {
				return -errno;
			}
			close(fd);
#ifdef debug
			end
#endif
			return JK_SUCCESS;
		}
		// if ((res = read(fd, &stbuf, sizeof(stbuf))) != sizeof(stbuf)) {
        //     return -errno;
        // }
        memcpy(&(stbuf.st_atime), ts, sizeof(*ts));
        memcpy(&(stbuf.st_mtime), ts + 1, sizeof(*ts));
        if ((res = write(fd, &stbuf, sizeof(stbuf))) != sizeof(stbuf)) {
            close(fd);
            return -errno;
        }
        close(fd);
#ifdef debug
		end
#endif
        return JK_SUCCESS;
    } else {
        res = utimensat(0, ssdpath, ts, AT_SYMLINK_NOFOLLOW);
        if (res == -1) {
            return -errno;
        }
#ifdef debug
		fprintf(dfp, "hit here!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		end
#endif
        return JK_SUCCESS;
    }
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
    fd = jk_open(path, fi);
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
    // .link		= jk_link,
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
	for (int i = 0; i < 128; i++) {
		realfd[i] = -1;
	}
#ifdef debug
	strcpy(debugpath, "/home/dio/Documents/jkfs/src/debug.log");
	unlink(debugpath);
#endif
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


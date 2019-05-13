#!/bin/bash


# set up environment
set +h
umask 022

export LJOS=/home/dio/buildlinux
mkdir -pv ${LJOS}
export LC_ALL=POSIX
export PATH=${LJOS}/cross-tools/bin:/bin:/usr/bin

mkdir -pv ${LJOS}/{bin,boot{,/grub},dev,{etc/,}opt,home,lib/{firmware,modules},lib64,mnt}
mkdir -pv ${LJOS}/{proc,media/{floppy,cdrom},sbin,srv,sys}
mkdir -pv ${LJOS}/var/{lock,log,mail,run,spool,opt,cache,lib/{misc,locate},local}
install -dv -m 0750 ${LJOS}/root
install -dv -m 1777 ${LJOS}{/var,}/tmp
install -dv ${LJOS}/etc/init.d
mkdir -pv ${LJOS}/usr/{,local/}{bin,include,lib{,64},sbin,src}
mkdir -pv ${LJOS}/usr/{,local/}share/{doc,info,locale,man,misc,terminfo,zoneinfo}
mkdir -pv ${LJOS}/usr/{,local/}share/man/man{1,2,3,4,5,6,7,8}

for dir in ${LJOS}/usr{,/local}; do
	ln -sv share/{man,doc,info} ${dir}
done

# prepare for cross-compilation
install -dv ${LJOS}/cross-tools{,/bin}
ln -svf /proc/mounts ${LJOS}/etc/mtab

cat > ${LJOS}/etc/passwd << "EOF"
root::0:0:root:/root:/bin/ash
EOF

cat > ${LJOS}/etc/group << "EOF"
root:x:0:
bin:x:1:
sys:x:2:
kmem:x:3:
tty:x:4:
daemon:x:6:
disk:x:8:
dialout:x:10:
video:x:12:
utmp:x:13:
usb:x:14:
EOF

cat > ${LJOS}/etc/fstab << "EOF"
# file system  mount-point  type   options          dump  fsck
#                                                         order

rootfs          /               auto    defaults        1      1
proc            /proc           proc    defaults        0      0
sysfs           /sys            sysfs   defaults        0      0
devpts          /dev/pts        devpts  gid=4,mode=620  0      0
tmpfs           /dev/shm        tmpfs   defaults        0      0
EOF

cat > ${LJOS}/etc/profile << "EOF"
export PATH=/bin:/usr/bin

if [ `id -u` -eq 0 ] ; then
        PATH=/bin:/sbin:/usr/bin:/usr/sbin
        unset HISTFILE
fi


# Set up some environment variables.
export USER=`id -un`
export LOGNAME=$USER
export HOSTNAME=`/bin/hostname`
export HISTSIZE=1000
export HISTFILESIZE=1000
export PAGER='/bin/more '
export EDITOR='/bin/vi'
EOF

echo "ljos-test" > ${LJOS}/etc/HOSTNAME

cat > ${LJOS}/etc/issue<< "EOF"
Linux Journal OS 0.1a
Kernel \r on an \m

EOF

cat > ${LJOS}/etc/inittab<< "EOF"
::sysinit:/etc/rc.d/startup

tty1::respawn:/sbin/getty 38400 tty1
tty2::respawn:/sbin/getty 38400 tty2
tty3::respawn:/sbin/getty 38400 tty3
tty4::respawn:/sbin/getty 38400 tty4
tty5::respawn:/sbin/getty 38400 tty5
tty6::respawn:/sbin/getty 38400 tty6

::shutdown:/etc/rc.d/shutdown
::ctrlaltdel:/sbin/reboot
EOF

cat > ${LJOS}/etc/mdev.conf<< "EOF"
# Devices:
# Syntax: %s %d:%d %s
# devices user:group mode

# null does already exist; therefore ownership has to
# be changed with command
null    root:root 0666  @chmod 666 $MDEV
zero    root:root 0666
grsec   root:root 0660
full    root:root 0666

random  root:root 0666
urandom root:root 0444
hwrandom root:root 0660

# console does already exist; therefore ownership has to
# be changed with command
console root:tty 0600 @mkdir -pm 755 fd && cd fd && for x
 ↪in 0 1 2 3 ; do ln -sf /proc/self/fd/$x $x; done

kmem    root:root 0640
mem     root:root 0640
port    root:root 0640
ptmx    root:tty 0666

# ram.*
ram([0-9]*)     root:disk 0660 >rd/%1
loop([0-9]+)    root:disk 0660 >loop/%1
sd[a-z].*       root:disk 0660 */lib/mdev/usbdisk_link
hd[a-z][0-9]*   root:disk 0660 */lib/mdev/ide_links

tty             root:tty 0666
tty[0-9]        root:root 0600
tty[0-9][0-9]   root:tty 0660
ttyO[0-9]*      root:tty 0660
pty.*           root:tty 0660
vcs[0-9]*       root:tty 0660
vcsa[0-9]*      root:tty 0660

ttyLTM[0-9]     root:dialout 0660 @ln -sf $MDEV modem
ttySHSF[0-9]    root:dialout 0660 @ln -sf $MDEV modem
slamr           root:dialout 0660 @ln -sf $MDEV slamr0
slusb           root:dialout 0660 @ln -sf $MDEV slusb0
fuse            root:root  0666

# misc stuff
agpgart         root:root 0660  >misc/
psaux           root:root 0660  >misc/
rtc             root:root 0664  >misc/

# input stuff
event[0-9]+     root:root 0640 =input/
ts[0-9]         root:root 0600 =input/

# v4l stuff
vbi[0-9]        root:video 0660 >v4l/
video[0-9]      root:video 0660 >v4l/

# load drivers for usb devices
usbdev[0-9].[0-9]       root:root 0660 */lib/mdev/usbdev
usbdev[0-9].[0-9]_.*    root:root 0660
EOF

cat > ${LJOS}/boot/grub/grub.cfg<< "EOF"

set default=0
set timeout=5

set root=(hd0,1)

menuentry "Linux Journal OS 0.1a" {
        linux   /boot/vmlinuz-4.16.3 root=/dev/sda1 ro quiet
}
EOF

touch ${LJOS}/var/run/utmp ${LJOS}/var/log/{btmp,lastlog,wtmp}
chmod -v 664 ${LJOS}/var/run/utmp ${LJOS}/var/log/lastlog


# build the cross compiler
unset CFLAGS
unset CXXFLAGS

export LJOS_HOST=$(echo ${MACHTYPE} | sed "s/-[^-]*/-cross/")
export LJOS_TARGET=x86_64-unknown-linux-gnu
export LJOS_CPU=k8
export LJOS_ARCH=$(echo ${LJOS_TARGET} | sed -e 's/-.*//' -e 's/i.86/i386/')
export LJOS_ENDIAN=little

# kernel headers
cd /home/dio/Downloads
tar -xvf linux-4.14.117.tar.xz
cd linux-4.14.117
make mrproper
make ARCH=${LJOS_ARCH} headers_check && make ARCH=${LJOS_ARCH} INSTALL_HDR_PATH=dest headers_install
cp -rv dest/include/* ${LJOS}/usr/include

#binutils
cd /home/dio/Downloads
tar -xvf binutils-2.30.tar.xz
mkdir binutils-build
cd binutils-build
../binutils-2.30/configure --prefix=${LJOS}/cross-tools --target=${LJOS_TARGET} --with-sysroot=${LJOS} --disable-nls --enable-shared --disable-multilib
make configure-host && make
ln -sv lib ${LJOS}cross-tools/lib64
make install
cp -v ../binutils-2.30/include/libiberty.h ${LJOS}/usr/include

#gcc static
cd /home/dio/Dowloads
tar -xvf gcc-7.3.0.tar.xz
tar -xvf gmp-6.1.2.tar.xz
tar -zxvf mpc-1.1.0.tar.gz
tar -xvf mpfr-4.0.1.tar.xz
mv gmp.6.1.2 gcc-7.3.0/gmp
mv mpfr-4.0.1 gcc-7.3.0/mpfr
mv mpc-1.1.0 gcc-7.3.0/mpc
mkdir gcc-static
cd gcc-static

AR=ar LDFLAGS="-Wl,-rpath,${LJOS}/cross-tools/lib" \
../gcc-7.3.0/configure --prefix=${LJOS}/cross-tools \
--build=${LJOS_HOST} --host=${LJOS_HOST} \
--target=${LJOS_TARGET} \
--with-sysroot=${LJOS}/target --disable-nls \
--disable-shared \
--with-mpfr-include=$(pwd)/../gcc-7.3.0/mpfr/src \
--with-mpfr-lib=$(pwd)/mpfr/src/.libs \
--without-headers --with-newlib --disable-decimal-float \
--disable-libgomp --disable-libmudflap --disable-libssp \
--disable-threads --enable-languages=c,c++ \
--disable-multilib --with-arch=${LJOS_CPU}

make all-gcc all-target-libgcc && make install-gcc install-target-libgcc
ln -vs libgcc.a `${LJOS_TARGET}-gcc -print-libgcc-file-name |sed 's/libgcc/&_eh/'`

# glibc
cd /home/dio/Downloads
tar -xvf glibc-2.27.tar.xz
mkdir glibc-build
cd glibc-build

echo "libc_cv_forced_unwind=yes" > config.cache
echo "libc_cv_c_cleanup=yes" >> config.cache
echo "libc_cv_ssp=no" >> config.cache
echo "libc_cv_ssp_strong=no" >> config.cache

BUILD_CC="gcc" CC="${LJOS_TARGET}-gcc" \
AR="${LJOS_TARGET}-ar" \
RANLIB="${LJOS_TARGET}-ranlib" CFLAGS="-O2" \
../glibc-2.27/configure --prefix=/usr \
--host=${LJOS_TARGET} --build=${LJOS_HOST} \
--disable-profile --enable-add-ons --with-tls \
--enable-kernel=2.6.32 --with-__thread \
--with-binutils=${LJOS}/cross-tools/bin \
--with-headers=${LJOS}/usr/include \
--cache-file=config.cache

make && make install_root=${LJOS}/ install

# gcc final
cd /home/dio/Downloads
mkdir gcc-build
cd gcc-build

AR=ar LDFLAGS="-Wl,-rpath,${LJOS}/cross-tools/lib" \
../gcc-7.3.0/configure --prefix=${LJOS}/cross-tools \
--build=${LJOS_HOST} --target=${LJOS_TARGET} \
--host=${LJOS_HOST} --with-sysroot=${LJOS} \
--disable-nls --enable-shared \
--enable-languages=c,c++ --enable-c99 \
--enable-long-long \
--with-mpfr-include=$(pwd)/../gcc-7.3.0/mpfr/src \
--with-mpfr-lib=$(pwd)/mpfr/src/.libs \
--disable-multilib --with-arch=${LJOS_CPU}
make && make install
cp -v ${LJOS}/cross-tools/${LJOS_TARGET}/lib64/libgcc_s.so.1 ${LJOS}/lib64

export CC="${LJOS_TARGET}-gcc"
export CXX="${LJOS_TARGET}-g++"
export CPP="${LJOS_TARGET}-gcc -E"
export AR="${LJOS_TARGET}-ar"
export AS="${LJOS_TARGET}-as"
export LD="${LJOS_TARGET}-ld"
export RANLIB="${LJOS_TARGET}-ranlib"
export READELF="${LJOS_TARGET}-readelf"
export STRIP="${LJOS_TARGET}-strip"

# build target image - busybox
cd /home/dio/Downloads
tar -xjf busybox-1.28.3.tar.bz2
cd busybox-1.28.3
make CROSS_COMPILE="${LJOS_TARGET}-" defconfig
make CROSS_COMPILE="${LJOS_TARGET}-"
make CROSS_COMPILE="${LJOS_TARGET}-" CONFIG_PREFIX="${LJOS}" install

cp -v examples/depmod.pl ${LJOS}/cross-tools/bin
chmod 755 ${LJOS}/cross-tools/bin/depmod.pl

# build target image - linux kernel
cd /home/dio/Downloads
cd linux-4.14.117
make ARCH=${LJOS_ARCH} CROSS_COMPILE=${LJOS_TARGET}- x86_64_defconfig

cp -v arch/x86/boot/bzImage ${LJOS}/boot/vmlinuz-4.16.3
cp -v System.map ${LJOS}/boot/System.map-4.16.3
cp -v .config ${LJOS}/boot/config-4.16.3

# build target image - the bootscripts
cd /home/dio/Downloads
tar -xjf clfs-embedded-bootscripts-1.0-pre5.tar.bz2
cd clfs-embedded-bootscripts-1.0-pre5
	#todo: edit Makefile here
make DESTDIR=${LJOS}/ install-bootscripts
ln -sv ../rc.d/startup ${LJOS}/etc/init.d/rcS

# build target iamge - zlib - not neccessary
cd /home/dio/Downloads
tar -zxvf zlib-1.2.11.tar.gz
cd zlib-1.2.11
sed -i 's/-O3/-Os/g' configure
./configure --prefix=/usr --shared
make && make DESTDIR=${LJOS}/ install
mv -v ${LJOS}/usr/lib/libz.so.* ${LJOS}/lib
ln -svf ../../lib/libz.so.1 ${LJOS}/usr/lib/libz.so
ln -svf ../../lib/libz.so.1 ${LJOS}/usr/lib/libz.so.1
ln -svf ../lib/libz.so.1 ${LJOS}/lib64/libz.so.1

# build target image - installing the target image
cp -rf ${LJOS}/ ${LJOS}-copy
rm -rfv ${LJOS}-copy/cross-tools
rm -rfv ${LJOS}-copy/usr/src/*

FILES="$(ls ${LJOS}-copy/usr/lib64/*.a)"
for file in $FILES; do
	rm -f $file
done

find ${LJOS}-copy/{,usr/}{bin,lib,sbin} -type f -exec sudo strip --strip-debug '{}' ';'
find ${LJOS}-copy/{,usr/}lib64 -type f -exec sudo strip --strip-debug '{}' ';'

sudo chown -R root:root ${LJOS}-copy
sudo chgrp 13 ${LJOS}-copy/var/run/utmp ${LJOS}-copy/var/log/lastlog
sudo mknod -m 0666 ${LJOS}-copy/dev/null c 1 3
sudo mknod -m 0600 ${LJOS}-copy/dev/console c 5 1
sudo chmod 4755 ${LJOS}-copy/bin/busybox

cd ${LJOS}-copy/
sudo tar cfJ ../ljos-build-21April2018.tar.xz *


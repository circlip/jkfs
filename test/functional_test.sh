#!/bin/sh

# this happens in ssd
echo ======= CREATE/DELETE R/W SYMLINK COPY =======
echo ================ START TEST 1 ================
echo 
# create a directory
echo \#: mkdir dir1
mkdir -v dir1
echo 

# create a file
echo \#: touch file1
touch file1
echo 

# write
echo '#: echo hello world > file1'
echo hello world > file1
echo 

# read
echo \#: cat file1
cat file1
echo 

# create symlink
echo \#: ln -s file1 slnk1
ln -sv file1 slnk1
echo 

# copy
echo \#: cp file1 dir1/file2
cp -v file1 dir1/file2
echo 

# get attribute
echo \#: ls -l
ls -l
echo 

# tree
echo \#: tree
tree
echo 

echo ============== END OF TEST 1 =================

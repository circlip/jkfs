echo ============= TAR AND UNTAR ===============
echo ============= START TEST 2 ================
echo

echo \#: cp /home/dio/Downloads/test.tar.gz te.tar.gz
cp -v /home/dio/Downloads/test.tar.gz te.tar.gz
echo
 
echo \#: tree
tree
echo

echo \#: tar -zxvf te.tar.gz
tar -zxvf te.tar.gz
echo

echo \#: tree
tree
echo

echo \#: tar -cJvf te2.tar.xz test/
tar -cJvf te2.tar.xz test/
echo

mkdir decompress
echo \#: tar -xvf te2.tar.xz --directory=decompress/
tar -xvf te2.tar.xz --directory=decompress/
echo

tree test/ decompress/test

echo ============= END OF TEST 2 ================

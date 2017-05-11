Lask
=====

### 编译和安装

lask提供了openwrt的Makefile(Makefile.openwrt)， 将其重命名为Makefile即可按openwrt的方式进行编译。lask位于menuconfig的languages/lua菜单下。


##### 安装lua
当前未安装lua，或者lua并非以动态库的方式安装。
<pre>
cd lua
./make.sh linux
sudo ./make.sh install
</pre>


##### 安装lask
<pre>
make
sudo make install
</pre>

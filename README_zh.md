# Lask
-----
为lua封装了posix接口并提供了一个异步通信框架。

# 编译和安装

### OpenWrt
-----
lask提供了openwrt的Makefile(Makefile.openwrt)， 将其重命名为Makefile即可按openwrt的方式进行编译。lask位于menuconfig的languages/lua菜单下。

### 非OpenWrt
-----

##### 安装lua
当前未安装lua，或者lua并非以动态库的方式安装。
~~~
cd lua
./make.sh linux
sudo ./make.sh install
~~~

##### 安装lask
~~~
make
sudo make install
~~~

# 参考手册

## 接口约定

* Lask的最底层是std，std主要封装了posix接口，并提供了buffer和reader对象。std导出的包都是全局可见的。
也只有std会修改全局名字空间。

* 虽然tasklet，log等在被加载时会自行require 'std'，但cjson,zlib如果早于std被加载则会报错。
因此建议总是一开始就require 'std'。

* 文件描述符作为lua number类型直接暴露而不是在此之上提供如文件对象之类的抽象。

* errno总是作为返回值之一，如：pid, err = os.fork()。

* 返回值如果是GC对象则可能是nil，但非GC对象则总是本类型的值。如os.open，成功时返回(>=0, 0)，
错误时返回(<0, >0)，而不是(nil, >0)；而fs.stat，成功时返回(table, 0)，错误时返回(nil, >0)。

## 文档约定

* 以下文档中函数的原型声明写为： ret1:type, ret2:type ... = funcname(arg1:type, ...)，如:
fd:number, err:number = os.open(filepath:string, flags:number, mode:number)。类型
可以是nil/number/string/table/any/function/userdata/buffer/reader。buffer和reader本质
上是userdata，但它们在lask中使用广泛，因此单独列出。any表示此参数或者返回值的类型是多变的，
亦或者这是个不透明的数据结构。如果一个参数可以是type1或者type2， 则写作： arg1:type1/type2。

* 如果函数的第一个参数是self:GC_TYPE(可GC的类型，如table, userdata)， 则此函数支持面向对象的语法糖。

* 本文档以package为单元，每个package列出了其常量和函数。如果没有特殊说明，则访问方式为pkgname.CONST，
pkgname.funcname，比如errno.EFBIG， errno.strerror()。

## errno

### 常量
-----
EFBIG
ECHILD
ENOSR
ENAMETOOLONG
ENOMEM
ENOPROTOOPT
ECONNABORTED
E2BIG
EADDRNOTAVAIL
ENOTTY
ENOBUFS
EBADMSG
EBUSY
ENOSYS
ENOTCONN
EROFS
EISCONN
EALREADY
EMLINK
EMSGSIZE
ENETRESET
ECONNRESET
ESTALE
EDEADLK
EACCES
ENOSTR
ESPIPE
EIO
ENETUNREACH
ETIME
EAFNOSUPPORT
EDQUOT
EOPNOTSUPP
ENFILE
EMULTIHOP
ESRCH
EBADF
EISDIR
EDESTADDRREQ
ENOSPC
EINPROGRESS
ENETDOWN
EILSEQ
ERANGE
EFAULT
EXDEV
ENOLINK
EPERM
EAGAIN
ENOENT
ENOLCK
EEXIST
EOVERFLOW
EINVAL
ETIMEDOUT
EPROTOTYPE
EHOSTUNREACH
EPROTONOSUPPORT
EADDRINUSE
ENODEV
EPROTO
EPIPE
EWOULDBLOCK
EDOM
ENOTEMPTY
ENXIO
ECONNREFUSED
ENOTSUP
ENOTSOCK
EIDRM
ENOTDIR
ETXTBSY
ENODATA
ECANCELED
ENOMSG
ENOEXEC
EMFILE
ELOOP
EINTR

### strerror
-----
errmsg:string = strerror([err:number])

_**描述**_:  获得错误的描述信息。

## buffer

buffer提供了一个可以从尾部写入，从头部读取的缓存。读操作(getxxxx)会使数据头部向后移动即减少了数据长度。
写操作(putxxxx)会使写数据尾部向后移动即增加数据长度。

本质上buffer是一个userdata。可通过`#`操作获取buffer的数据长度。
buffer也自定义了__tostring元方法，其输出形式为"buffer (0x24a3368, memsiz=4096, datasiz=0, big-endian)"。
memsiz为buffer已经分配的内存空间大小（字节数）。datasiz为buffer的数据大小（字节数）。

### new
-----
buf:buffer = new([minsize:number])

_**描述**_:  创建一个buffer。

##### *参数*
*minsize*: 最小内存尺寸。默认4K。初始状态下并未申请内存。

##### *返回值*
*buf*: buffer对象。

### str
-----
str:string = str(self:buffer)

_**描述**_:  将buffer的所有内容转换成string。未影响buffer的数据。

### reader
-----
rd:reader = reader(self:buffer, [rd:reader[, offset:number[, length:number]]])

_**描述**_:  产生一个基于此buffer的reader对象。

##### *参数*
*rd*: 若提供则buffer重新初始化此reader对象。否则返回本buffer的内置reader。  
*offset*: 偏移。默认0。  
*length*: 长度。默认从offset到结尾的所有。

### setbe
setbe(self:buffer, be:boolean)

_**描述**_: 设置大小端。

##### *参数*
*be*: `true`大端，`false`小端。

### getline
-----
line1:string, ... = getline(self:buffer, num:number)

_**描述**_: 读取一行或者多行。

##### *参数*
*num*: 要读取的行数。注意返回值的数目可能少于num。返回的数据不包含'[\r]\n'。

### getlstr
-----
str:string = getlstr(self:buffer[, len:number])

_**描述**_: 读取指定字节数为string。

##### *参数*
*num*: 字节数。默认是整个buffer的剩余长度。

### getb
-----
b1:number, , ...  = getb(self:buffer[, num:number])

_**描述**_: 读取多个字节(uint8)。每个字节值作为number返回。

### getw
-----
w1:number, , ...  = getw(self:buffer[, num:number])

_**描述**_: 读取多个无符号短整型(uint16)。每个字节值作为number返回。受到setbe的影响。

### geti
-----
i1:number, ...  = geti(self:buffer[, num:number])

_**描述**_: 读取多个整型(int32)。每个字节值作为number返回。

### getlist
-----
val1:number, ... = getlist(format:string)

_**描述**_: 读取多个整数值。

##### *参数*
*format*: 更复杂的情况请结合string.unpack一起使用。格式：
* 'b' uint8。
* 'w' uint16。
* 'i' int32。

### fill
-----
self:buffer = fill(self:buffer, value:number, len:number)

_**描述**_: 填充buffer。

##### *参数*
*value*: 填充值。取低8位。  
*len*: 填充长度。

### putb
-----
self:buffer = putb(self:buffer, b1:number, b2:number, ...)

_**描述**_: 写入多个字节。

### putw
-----
self:buffer = putw(self:buffer, w1:number, w2:number, ...)

_**描述**_: 写入多个无符号短整型。

### puti
-----
self:buffer = puti(self:buffer, i1:number, i2:number, ...)

_**描述**_: 写入多个整型。

### putlist
-----
self:buffer = putlist(self:buffer, format:string,  ...)

_**描述**_: 写入多个整数。参考getlist。更复杂的情况请结合string.pack一起使用。

### putstr
----
self:buffer = putstr(self:buffer, str1:string, str2:string, ...)

_**描述**_: 写入多个string。

### putreader
-----
self:buffer = putreader(self:buffer, rd:reader)

_**描述**_: 写入reader的内容。

### overwrite
-----
self:buffer = overwrite(self:buffer, offset:number, val:number)

_**描述**_: 在指定偏移出写入数据(整型)。

### pop
-----
self:buffer = shift(self:buffer, len:number)

_**描述**_: 从尾部移除长度为len的数据。

### shift
-----
self:buffer = shift(self:buffer, len:number)

_**描述**_: 从头部移除长度为len的数据。

### rewind
-----
self:buffer = reset(self:buffer)

_**描述**_: 清空buffer的数据。

### reset
-----
self:buffer = reset(self:buffer)

_**描述**_: 清空buffer的数据并将内容回退到最小尺寸。

## reader
reader是buffer的读取器，提供了对buffer数据的读取操作。读取时会使reader的读游标向后移动即
减少可读数据。

本质上reader是一个userdata。可通过`#`获得reader的剩余数据长度。reader也自定义了__tostring
元方法，其输出形式为"reader (0x24a4da8, all=13, left=13, big-endian)"。all为总数据大小（字节数），
left为还未读取的数据大小（字节数）。

reader读取数据后不会影响buffer：
~~~
buf = buffer.new()
buf:putstr('hello, world\n')
print(buf:getline())   -- 显示'hello, world'
print(buf)             -- 显示'buffer (0x24a3368, memsiz=4096, datasiz=0, big-endian)'

buf:putstr('hello, world\n')
rd = buffer:reader()
print(rd:getline())    -- 显示'hello, world'
print(buf)             -- 显示'buffer (0x24a3368, memsiz=4096, datasiz=13, big-endian)'
~~~

##### *一边读取reader一边读写buffer会产生不可预料的后果。*

每个buffer都有一个内置的reader，不带参数多次调用buffer:reader得到的其实是同一个reader对象。

### setbe
----
setbe(self:reader, big_endian:boolean)

### str
-----
str:string = str(self:reader)

_**描述**_: 将所有可读数据转换成string

### getlstr
-----
str:string = getlstr(self:reader, len:number)

### getline
-----
str1:string, ... = getline(self:reader, num:number)

### getb
-----
b1, ... = getb(self:reader[, num:number])

### getw
-----
w1, ... = getw(self:reader[, num:number])

### geti
-----
i1, ... = geti(self:reader[, num:number])

### getlist
-----
val1, ... = getb(self:reader, format:string)

### skip/shift
-----
skip(num:number)

### shifted
-----
num:number = shifted(self:reader)

_**描述**_: 已经读取的字节数（all-left）。

## os

### 常量
-----
O_RDONLY
O_WRONLY
O_RDWR
O_APPEND
O_CREAT
O_EXCL
O_NONBLOCK
O_DSYNC
O_ASYNC
O_RSYNC
O_SYNC
O_TRUNC
O_CLOEXEC
SEEK_CUR
SEEK_SET
SEEK_END
WNOHANG
WUNTRACED

### open
-----
fd:number, err:number = open(path:string, flags:number, mode:number)

### creat
-----
fd:number, err:number = creat(path:string, mode:number)

### close
-----
err:number = close(fd:number)

### read
-----
data:string, err:number  = read(fd:number[, num_bytes:number])

_**描述**_: 读取数据并将其作为string返回。num_bytes可选，未提供时读取所有可读数据。

##### *返回值*
*data*: 读取到的数据。EOF或者错误时返回nil。  
*err*: 错误码。EOF对应0。  
错误码不会是EINTR/EAGAIN/EWOULDBLOCK，对于EINTR，read自动重新读取。对于EAGAIN/EWOULDBLOCK，
read返回0。 readb/write/writeb同样采用此操作。

### readb
-----
nread:number, err:number  = read(fd:number, buf:buffer[, num_bytes:number])

_**描述**_: 读取数据并将其写入buffer。num_bytes可选，未提供时读取所有可读数据。

### getnread
-----
num_bytes:number = getnread(fd:number)

_**描述**_: 获得可读字节数。

### readlink
-----
filepath:string, err:number = readlink(linkpath:string)

### write
-----
nwritten:number, err:number = write(fd:number, str1:string, ...)

_**描述**_: 向fd中写入一个或者多个字符串。

##### *返回值*
*nwritten*: 写入的数据字节数。  
*err*: 错误码。  

### writeb
-----
nwritten:number, err:number = writeb(fd:number, data:buffer/reader[, offset:number[, len:number]])

_**描述**_: 向fd中写入buffer/reader中的数据。

##### *参数*
*fd*: 文件描述符。  
*data*: 数据区。  
*offset*: 要写入的数据在数据区的偏移。默认0。  
*len*: 要写入的数据长度。默认到数据区尾部。  

##### *返回值*
*nwritten*: 写入的数据字节数。  
*err*: 错误码。  

### lseek
-----
currpos:number, err:number = lseek(fd:number, offset:number, whence:number)

### fsync
-----
err:number = fsync(fd:number)

### fdatasync
-----
err:number = fdatasync(fd:number)

### truncate
-----
err:number = truncate(filepath:string, length:number)

### ftruncate
-----
err:number = truncate(fd:number, length:number)

### dup
-----
new_fd:number, err:number = dup(fd:number)

### dup2
-----
new_fd:number, err:number = dup(fd:number, fd2:number)

### closerange
-----
closerange(fd1:number, fd2:number)

_**描述**_: 关闭[fd1, fd2]之间的所有fd。

### setnonblock
-----
setnonblock(fd:number)

### setcloexec
-----
setcloexec(fd:number)

### fork
-----
pid:number, err:number = fork()

_**描述**_: 出错时pid<0。 err为错误码。

### vfork
-----
pid:number, err:number = vfork()

### pipe
-----
rfd:number, wfd:number, err:number = pipe()

_**描述**_: 出错时rfd<0， wfd<0。

### setsid
-----
pid:number = setsid()

_**描述**_: 出错时pid<0， 错误原因为权限不够。

### getpid
-----
pid:number = getpid()

### getppid()
-----
pid:number = getppid()

### getuid
-----
uid:number = getuid()

### geteuid()
-----
uid:number = geteuid()

### getgid()
-----
gid:number = getgid()

### getegid()
-----
gid:number = getegid()

### setuid
-----
err:number = setuid(uid:number)

### seteuid()
-----
err:number = geteuid(uid:number)

### setgid()
-----
err:number = setgid(gid:number)

### setegid()
-----
err:number = setegid(gid:number)

### wait
-----
pid:number, stat/err:number = wait()

_**描述**_: 成功时pid>0，此时stat为进程退出状态。否则pid<0， err为错误码。

### waitpid
-----
pid:number, stat/err:number = waitpid(pid:number[, flags:number])

_**描述**_: flags可选。默认0。

### execl
-----
err:number = execl(path:string, arg1:string, ...)

_**描述**_: 最大支持31个参数。

## fs

### 常量
-----
ST_DEV
ST_RDEV
ST_INO
ST_NLINK
ST_ATIME
ST_CTIME
ST_MTIME
ST_MODE
ST_UID
ST_GID
ST_SIZE
ST_RDONLY
ST_NOSUID
F_OK
R_OK
W_OK
X_OK

### dir
-----
iter:functoin, ud:userdata = dir(path:string)
更一般的用法是：for each in dir(path) do ...  end

_**描述**_:  枚举目录下的所有文件。自动跳过`.`和`..`。

##### *参数*
*path*: 要遍历的目录。默认当前目录。

### listdir
-----
namelist:table, err:number = listdir(path:string)

_**描述**_:  获得目录下的文件名列表。自动跳过`.`和`..`。

##### *参数*
*path*: 目的目录。默认当前目录。

##### *返回值*
*namelist*: 文件名列表。  
*err*: 错误码。  

### glob
-----
iter:functoin, ud:userdata = glob(pattern:string)
更一般的用法是： for each in glob(pattern) do ... end

_**描述**_:  枚举目录下的所有文件。自动跳过`.`和`..`。

##### *参数*
*pattern*: 要遍历的目录。

### getcwd
-----
dir:string = getcwd()

### mkdir
-----
err:number = mkdir(dir:string)

### rmdir
-----
err:number = rmdir(dir:string)

### mkdir_p
-----
err:number = mkdir_p(dir:string)

_**描述**_: 逐级生成所有目录。

### mkfifo
-----
err:number = mkfifo(filepath:string)

### chdir
-----
err:number = fchdir(fd:number)

### fchdir
-----
err:number = chdir(dir:string)

### link
-----
err:number = link(filepath:string, linkpath:string)

### symlink
-----
err:number = symlink(filepath:string, linkpath:string)

### rename
-----
err:number = rename(old:string, new:string)

### remove
-----
err:number = remove(filepath:string)

### unlink
-----
err:number = unlink(path:string)

### access
-----
err:number = access(path:string, mode:number)

### basename
-----
name:string = basename(path:string)

### dirname
-----
name:string = dirname(path:string)

### realpath
-----
path:string = realpath(path:string)

### stat
-----
stinfo:table, err:number = stat(path:string)

##### *返回值*
*stinfo*:  {size=, uid=, gid=, ...}，详细字段列表见posix struct stat（去除了st_前缀）  
*err*: 错误码。

### fstat
-----
stinfo:table, err:number = fstat(fd:number)

### lstat
-----
stinfo:table, err:number = lstat(path:string)

### statvfs
-----
stinfo:table, err:number = statvfs(path:string)

##### *返回值*
*stinfo*:  {bsize=, bloks=,  ...}，详细字段列表见posix struct statvfs（去除了f_前缀）。  
*err*: 错误码。

### fstatvfs
-----
stinfo:table, err:number = statvfs(fd:number)

### chown
----
err:number = chown(filepath:string, uid:number, gid:number)

### fchown
----
err:number = chown(fd:number, uid:number, gid:number)

### lchown
----
err:number = lchown(filepath:string, uid:number, gid:number)

### chmod
-----
err:number = chmod(filepath:string, mode:number)

### fchmod
-----
err:number = chmod(fd:number, mode:number)

### utimes
-----
err:number = utimes(filepath:string, atime:number, mtime:number)

### umask
-----
old_mode:number = umask(mode:number)

### isreg/isdir/ischr/isblk/isfifo/issock/islnk
-----
yes:boolean = isreg(filepath:string)

### exists
-----
yes:boolean = exists(filepath:string)

## stat

### 常量
S_IFREG
S_IFDIR
S_IRWXU
S_IRWXO
S_IFLNK
S_IWGRP
S_IFIFO
S_ISVTX
S_ISGID
S_ISUID
S_IRWXG
S_IXOTH
S_IWOTH
S_IROTH
S_IXGRP
S_IFSOCK
S_IWUSR
S_IRGRP
S_IXUSR
S_IFCHR
S_IRUSR
S_IFBLK

### isreg/isdir/ischr/isblk/isfifo/issock/islnk
-----
yes:boolean = isreg(mode:number)

## time

### sleep
-----
time.sleep(sec:number)

_**描述**_: sleep。sec可精确到小数点后6位。

### time
-----
tm:number = time()

_**描述**_: 当前时间。获得浮点数。

### time2
-----
sec_since_unix_epoch:number, nano_sec:number = time()

_**描述**_: 当前时间。获得2个整数。

### uptime
-----
tm:number = uptime()

_**描述**_: 系统启动以来的时间。获得浮点数。

### uptime2
-----
sec_since_booted:number, nano_sec:number = uptime2()

_**描述**_: 系统启动以来的时间。获得2个整数。

### locatime
-----
timeinfo:table = localtime([now:number])

_**描述**_: 将本地时间拆成详情形式。

##### *返回值*
*timeinfo*: 详情。字段为： (year, mon, mday, hour, min, sec, wday, yday, isdst)。

### gmtime
-----
timeinfo:table = gmtime([now:number])

_**描述**_: 将UTC时间拆成详情形式。

### strftime
-----
str:string = time.strftime(fmt:string, clock:number)

### strptime
-----
clock:number = time.strptime(str:string, fmt:string)

## socket

### 常量
AF_INET
AF_INET6
AF_UNIX
AF_PACKET
AF_UNSPEC
SOCK_STREAM
SOCK_DGRAM
SOCK_RAW
SOCK_SEQPACKET
SOL_SOCKET
SOL_IP
SOL_TCP
SO_ACCEPTCONN
SO_BROADCAST
SO_DEBUG
SO_DONTROUTE
SO_ERROR
SO_KEEPALIVE
SO_LINGER
SO_OOBINLINE
SO_RCVBUF
SO_RCVLOWAT
SO_RCVTIMEO
SO_REUSEADDR
SO_SNDBUF
SO_SNDLOWAT
SO_SNDLOWAT
TCP_NODELAY
TCP_KEEPIDLE
TCP_KEEPCNT
TCP_KEEPINTVL
IP_FREEBIND
IP_TRANSPARENT
IPPROTO_IP
IPPROTO_IPV6
IPPROTO_ICMP
IPPROTO_UDP
IPPROTO_TCP
IPPROTO_RAW
SO_TYPE
SOMAXCONN
MSG_OOB
MSG_PEEK
MSG_DONTROUTE
SHUT_RD
SHUT_WR
SHUT_RDWR

### socket
-----
fd:number, err:number = socket(family:number, type:number[, protocol:number])

### connect
-----
err:number = connect(fd:number, addr:string, port:number)

### bind
-----
err:number = bind(fd:number, addr:string, port:number)

### listen
-----
err:number = listen(fd:number[, backlog:number])

_**描述**_:  backlog可选。默认SOMAXCONN。

### accept
-----
fd:number, addr:string, port:number, err:number = accept(fd:number)

_**描述**_: 出错时fd<0。

### recvfrom
-----
data:string, addr:string, port:number, err:number = recvfrom(fd:number)

_**描述**_: 即使fd处于阻塞模式本函数也不会阻塞。

### recvfromb
-----
nread:number, addr:string, port:number, err:number = recvfromb(fd:number, buf:buffer)

_**描述**_: 即使fd处于阻塞模式本函数也不会阻塞。

### sendto
-----
nwritten:number, err:number = sendto(fd:number, addr:string, port:number, data:string)

### sendtob
-----
nwritten:number, err:number = sendtob(fd:number, addr:string, port:number, data:buffer/reader[, offset:number[, len:number]])

### getpeername
-----
addr:string, port:number, err:number = getpeername(fd:number)

### getsockname
-----
addr:string, port:number, err:number = getsockname(fd:number)

### setsocketopt
-----
err:number = setsocketopt(fd:number, optname:number, optvalue:any)

_**描述**_: 设置套接字SOL_SOCKET层的选项。

##### *参数*
*fd*: 文件描述
*optname*: 选项名称。见常量表中SO_XXX等字段。  
*optvalue*: 选项值。不同optname有不同的类型：  
* SO_LINGER: table， 具体为{[1]=onoff:boolean, [2]=linger:number(integer)}
* SO_RCVTIMEO/SO_SNDTIMEO: number(double)
* SO_TYPE/SO_ERROR/SO_RCVBUF/SO_RCVLOWAT/SO_SNDBUF/SO_SNDLOWAT: number(integer)
* SO_BROADCAST/SO_DONTROUTE/SO_KEEPALIVE/SO_OOBINLINE/SO_REUSEADDR: boolean

### getsocketopt
-----
optvalue:any, err:number = setsocketopt(fd:number, optname:number)

_**描述**_: 获取套接字SOL_SOCKET层的选项。详情见setsocketopt。

### setipopt
-----
@TODO

### getipopt
-----
@TODO

### settcpopt
-----
err:number = settcpopt(fd:number, optname:number, optvalue:any)

_**描述**_: 设置套接字SOL_TCP层的选项。

##### *参数*
*fd*: 文件描述
*optname*: 选项名称。见常量表中TCP_XXX等字段。  
*optvalue*: 选项值。不同optname有不同的类型：  
* TCP_KEEPCNT/TCP_KEEPIDLE/TCP_KEEPINTVL: number(integer)
* TCP_NODELAY: boolean

### gettcpopt
-----
optvalue:any, err:number = gettcpopt(fd:number, optname:number)

_**描述**_: 获取套接字SOL_TCP层的选项。详情见settcpopt。

### shutdown
-----
err:number = shutdown(fd:number, shut:number)

### tcpserver
-----
fd:number, err:number = tcpserver(addr:string, port:number[, backlog:number])

_**描述**_: 快速创建tcp服务器。

##### *参数*
*addr*: 本地地址。`nil`表示'0.0.0.0'。自动检测ipv4和ipv6。
*port*: 本地端口。
*backlog*: 可选。

##### *返回*
*fd*: 已创建的的服务器套接字。
*err*: 错误码。

### unserver
-----
fd:number, err:number = unserver(path:string)

_**描述**_: 快速创建unix套接字服务器。

##### *参数*
*addr*: 路径。

##### *返回*
*fd*: 已创建的的服务器套接字。
*err*: 错误码。

## poll

封装了IO复用的系统调用，如poll/epoll。

### 常量
IN
OUT
ERR
HUP
ET

### create
-----
handle:any, err:number = create()

_**描述**_: 创建一个IO复用的句柄。目前封装的是epoll，因此handle实际上是fd。

### add
-----
err:number = add(handle:any, fd:number, events:number)

_**描述**_: 添加事件处理。events是IN/OUT/ET的组合。

### mod
-----
err:number = mod(handle:any, fd:number, events:number)

### del
-----
err:number = del(handle:any, fd:number)

### wait
-----
results:table, err:number = wait(handle:any[, sec:number])

_**描述**_: 进行一次事件等待。返回的results为: {[fd1]=events1, ... , [fdN]=eventsN}
eventsi是IN/OUT/HUP/ERR的组合，且若HUP/ERR存在则IN必然存在。

### destroy
-----
destroy(handle:any)

_**描述**_: 销毁此句柄。为了更好的扩展性不建议直接使用close。

### select
-----
rset:table/nil, wset:table/nil, eset:table/nil, err:number = select(rset:table/nil, wset:table/nil, eset:table/nil[, sec:number])

_**描述**_: rset/wset/eset均为{fd1, fd2, ..., fdN}的数组。

### waitfd
-----
revents:string = waitfd(fd:number, events:string[, sec:number])

_**描述**_: 等待单个fd上的IO事件。

##### *参数*
*fd*: 文件描述符。  
*events*: 'r'/'w'/'rw'。  
*sec*: 超时时间。可选。

##### *返回值*
*revents*: 'r'/'w'/'rw'/''。

## netdb

### 常量
-----
HOST_NOT_FOUND
TRY_AGAIN
NO_RECOVRERY
EAI_AGAIN
EAI_BADFLAGS
EAI_FAIL
EAI_FAMILY
EAI_MEMORY
EAI_NONAME
EAI_OVERFLOW
EAI_SERVICE
EAI_SOCKTYPE
EAI_SYSTEM

### strerror
-----
errmsg:string = strerror(err:number)

### getaddrbyname
-----
addrlist:table, err:number = getaddrbyname(hostname:string)

_**描述**_: 域名查询。

##### *参数*
*hostname*: 主机名称。

##### *返回值*
*addrlist*: 地址列表。每个元素为ipv4或者ipv6。  
*err*: 错误码。

## signal

### 常量
-----
SIGKILL
SIGVTALRM
SIGCHLD
SIGBUS
SIGUSR1
SIGSTOP
SIGHUP
SIGTTOU
SIGQUIT
SIGPIPE
SIGCONT
SIGILL
SIGTTIN
SIGTERM
SIGALRM
SIGABRT
SIGPROF
SIGSEGV
SIGTSTP
SIGUSR2
SIGINT

### signal
-----
old_handler:function/string = signal(sig:number, handler:function/string)

_**描述**_: 注册信号处理函数。

##### *参数*
*sig*: 信号值。  
*handler*: 处理函数。特别的，'default'对应SIG_DFL， 'ignore'对应SIG_IGN。

##### *返回值*
*old_handler*: 上一个处理函数。也可能是'default'或者'ignore'。

### alarm
-----
alarm(sec:number)

### kill
-----
err:number = kill(pid:number, sig:number)

### raise
-----
raise(sig:number)

## std导出的其他全部变量和函数

### NULL
-----
_**描述**_:  只读的空table。

### tmpbuf
-----
_**描述**_:  公共buffer对象。约定由使用者在使用前初始化（一般是rewind）。

### dummy
-----
_**描述**_:  空函数。

### dump
----
dump(value:any[, level:number])

_**描述**_:  打印一个值。支持对table的深层打印。自动检测循环引用。

##### *参数*
*value*: 要打印的值。  
*level*: 深入到table的层次数。可选。默认100。

### getopt
-----
opts:table = getopt(argv:table, short:string, long:table)

_**描述**_: 解析命令行参数。

##### *参数*
*argv*: 命令行参数。  
*short*: 短参。如'hvs:p:'对应'-h -v -s www.qq.com -p 80'。  
*long*: 长参。如{'help', 'version', 'server=', 'port='}对应'--help --version --server=www.qq.com --port=80'。

##### *返回值*
*opts*: 解析结果。如{h=true, help=true, server='www.qq.com', port='80'}。解析失败则为nil。

### daemonize
-----
daemonize([openmax:number])

_**描述**_: 变成守护进程。

##### *参数*
*openmax*: 关闭[0, openmax]范围内的所有文件描述符。可选。默认64。

## log

提供了日志记录的功能。使用require 'log'来加载。
~~~
log.lua中只有一行代码： return require('_log')('default')
因此载入log.lua实际上是获得一个名为'default'的默认日志对象。
可以通过调用如local mylog = require('_log')('mylog')来产生更多的log对象。
~~~

### init
-----
init(conf:table)

_**描述**_:  初始化。

##### *参数*
*conf*: 日志的配置信息。可用的字段如下。
* level: string类型，取"debug"/"info"/"warn"/"error"/"fatal"其中之一。日志级别。
* withtime: boolean类型。是否加上时间戳。
* withcolor: boolean类型。是否加上控制台颜色(只对输出到控制台和socket有效)
* path: string类型。日志输出的文件路径。"null"表示不产生日志。"stdout"表示输出到控制台。
* flimit: string类型。限定文件的最大长度。只对输出到常规文件有效。示例："4000000", "4000k", "4m"。

### debug
-----
debug(...)

_**描述**_:  记录一条调试级别日志。

##### *参数*
*...*: 多个参数。每个参数的类型只能是string/number/nil。
~~~
所有参数从左至右依次输出。尾部自动加上换行符。
例如：
local name, girlfriend, 28 = 'spyderj', nil, 28
log.debug('name = ', name, ', girlfriend = ', girlfriend, ', age = ', age)
产生的日志内容部分为： 'name = spyderj, girlfriend = nil, age = 28\n'。
同样适用于info/warn/error/fatal。
~~~

### info
-----
info(...)

_**描述**_:  记录一条通知级别日志。

### warn
-----
warn(...)

_**描述**_:  记录一条警告级别日志。

### error
-----
error(...)

_**描述**_:  记录一条错误级别日志。

### fatal
-----
fatal(...)

_**描述**_:  记录一条致命级别日志。然后程序自动退出。

### reopen
-----
reopen()

_**描述**_: 重新打开日志文件。常用于日志切割。

### capture
-----
capture(sockpath:string[, level:string])

_**描述**_: 日志劫持。

##### *参数*
*sockpath*: 新的日志输出路径。此路径应当是一个unix套接字文件。  
*level*: 新的日志级别。默认为"debug"。

### release
-----
release(sockpath:string[, level:string])

_**描述**_: 释放劫持。

## tasklet

tasklet实现了一个基于事件和超时时间的协程调度器。协程被进一步封装成task。

### current_task
-----
task:table = current_task()

_**描述**_:  获取当前task。

##### *返回值*
*task*: 当前task。

### start_task
-----
task:table = start_task(func:function, obj:table/string/nil, joinable:boolean)

_**描述**_:  新建一个task并使之处于就绪状态。

##### *参数*
*func*: 任务的入口函数。  
*obj*: tasklet允许task对象与其他业务共享一个table（此时应避免使用't_'前缀的字段）。如果是string则指示task名称。  
*joinable*: 见join_tasks。

##### *返回值*
*task*: 创建（或者复用）的task对象。此task已经处于就绪状态。其中下列字段可见：
* t_name: task名称。
* sighandler: 在kill_task中被tasklet自动调用的函数。原型为sigahandler(task:table, sig:any)。此函数运行在被唤醒的task中且实现时切忌阻塞。

### reap_task
-----
reap_task(task:table)

_**描述**_:  终止并回收一个task。

##### *参数*
*task*: 目标task。

### kill_task
-----
kill_task(task:table, sig:any)

_**描述**_: 向目标task发送信号。此信号非unix信号，仅仅是tasklet提供的一种原始通信机制。
tasklet自动调用task.sighandler。

##### *参数*
*task*: 目标task。  
*sig*: 任意信号数据。  

### join_tasks
-----
join_tasks()

_**描述**_:  等待所有的joinable task退出。

### sleep
-----
err:number = sleep([sec:number])

_**描述**_:  使当前task休眠一段时间。

##### *返回值*
*err*: 一般返回0。如果是被kill_task唤醒则返回EINTR。

### add_handler
-----
add_handler(fd:number, events:number, handler:function)

_**描述**_: 添加事件处理函数。

##### *参数*
*fd*: 文件描述符。  
*events*: 事件类型。READ/WRITE/EDGE的组合。  
*handler*: 处理函数。原型为: handler(fd:number, events:number)。  

### del_handler
-----
del_handler(fd:number)

_**描述**_: 删除事件处理函数。

##### *参数*
*fd*: 文件描述符。

### mod_handler
-----
mod_handler(fd:number, events:number, handler:function)

_**描述**_: 修改事件处理函数。

##### *参数*
*fd*: 文件描述符。  
*events*: 事件类型。  
*handler*: 处理函数。  

### loop
-----
loop()

_**描述**_: 开始事件处理循环。

## tasklet.message_channel

message_channel用于task之间的通信。工作机制类似unix的fifo。

考虑task A和B使用messsage_channel进行通信的情况。
首先A调用message_channel.write向B发送消息，由于channel上没有读者，A进入阻塞状态。
接下来B调用了read，read直接返回A所发送的消息，并导致A变成就绪状态。
B继续调用read，此时channel上没有写者，B进入阻塞状态直到A调用write。A调用write，
write成功并直接返回。

可以为message_channel指定一个消息缓冲区。此时即使channel上没有读者，只要缓冲区未满write便立即成功。

message_channel也提供了post，此方法尝试发送并立刻返回是否发送成功的boolean值。

通过require 'tasklet.channel.message'来加载。接口在tasklet.message_channel名字空间中。

### new
-----
ch:table = new(capacity:number)

_**描述**_: 创建一个message_channel。

##### *参数*
*capacity*: 缓冲区能容纳的消息个数。

##### *返回值*
*ch*: 已创建的message_channel。

### write
-----
err:number = write(self:table, msg:any[, timedout_sec:number])

_**描述**_:  写消息。

##### *参数*
*msg*: 任意消息。  
*timedout_sec*: 超时时间，默认永久。  

##### *返回值*
*err*: 错误码。

### read
-----
msg:any, err:number = read(self:table[, timedout_sec:number])

_**描述**_:  读消息。

##### *参数*
*timedout_sec*: 超时时间，默认永久。

##### *返回值*
*msg*: 读取到的msg， 出错时为`nil`。  
*err*: 错误码。

### post
-----
succeed:boolean = post(self:table, msg)

_**描述**_:  投递msg，不论是否成功都立即返回。

##### *参数*
*msg*: 要投递的msg

##### *返回值*
*succeed*: 成功时返回`true`，否则`false`。

### close
-----
close(self:table)

_**描述**_:  关闭channel，将导致所有的read/write方法返回EPIPE。

## tasklet.stream_channel

stream_channel为数据流（包括但不限于SOCK_STREAM套接字，管道，串口）提供了一层抽象。
在读写此数据流时只会阻塞当前task，而非整个进程。

通过require 'tasklet.channel.stream'来加载。接口在tasklet.stream_channel名字空间中。

这里用示例来说明。
```lua
-- async_echo.lua: 回显用户的输入。每秒钟打印一次'dida'。
local tasklet = require 'tasklet.channel.stream'
tasklet.start_task(function ()
    while true do
        tasklet.sleep(1)
        print('dida')
    end
end)
tasklet.start_task(function ()
    local ch = tasklet.stream_channel.new(0) -- 0即STDIN
    while true do
        local input = ch:read()
        if not input then os.exit() end
        print(input)
    end
end)
tasklet.loop()
```

### new
-----
ch:table = new(value:number/string/function/nil[, rbufsiz:number])

_**描述**_: 创建。

##### *参数*
*value*: 指定的channel。
* number: channel绑定到一个已经打开的fd。
* string: 连接到子进程的管道。子进程中将string作为shell命令执行。
* function: 连接到子进程的管道。子进程中将继续执行function的代码，然后退出。
* nil: 空白状态的channel，一般接下来connect。

*rbufsiz*: channel的读取缓存大小。

##### *返回值*
*ch*: 创建好的table。
~~~
此table可复用但：
    避免使用'ch_'为前缀的字段
    禁止再设置metatable
ch有两个字段可见
    ch_fd     
    ch_state  >0 正常， ==0 EOF， <0 错误
~~~

### connect
-----
err:number = connect(self:table, addr:string, port:number, sec:number)

_**描述**_: 连接到一个tcp/unix服务端。

##### *参数*
*msg*: ip地址或者unix套接字路径  
*port*: 端口号，若未unix套接字则为0  
*sec*: 超时时间，默认一直等待直到成功连接或者网络出错  

##### *返回值*
*err*: 错误码

### read
-----
data:reader/string/nil, err:number = read(self:table, req:number/nil[, timedout_sec:number])

_**描述**_: 读数据。同一时刻只允许一个task读。

##### *参数*
*req*: 要读取的的字节数。
~~~
    nil: 读取一行数据，此时返回读到的字符串。
    number <0: 当可读数据量>=(-req)时，返回所有的可读数据。
    number >0: 读取(req)字节数。若超过rbufsiz则返回rbufsiz大小的内容。
~~~    
*timedout_sec*: 超时时间。

##### *返回值*
*data*: 读取到的数据
~~~
    reader: 当req为number时返回reader对象。
    string: 当req为nil时返回string对象。
    nil: 错误或者EOF。
~~~    
*err*: 错误码(对于EOF时err=0)

### write
-----   
err:number = write(self:table, data:buffer/reader[, timedout_sec:number])

_**描述**_: 写数据。同一时刻只允许一个task写。

##### *参数*
*data*: 要写的的数据。  
*timedout_esc*: 超时时间。  

##### *返回值*
*err*: 错误码。

### close
-----
close(self:table)
_**描述**_: 关闭channel。可导致read/write返回EBADF。


## tasklet.sslstream_channel

### 简介
类似stream_channel，提供了对SSL数据流的抽象。

通过require 'tasklet.channel.sslstream'来加载。接口在tasklet.sslstream_channel名字空间中。

### ctx
-----
若sslstream_channel.new中未提供ctx参数，则取sslstream_channel.ctx。

### new
-----
ch:table = new(value:number/nil, rbufsiz:number[, ctx:userdata])

_**描述**_: 创建。

##### *参数*
*value*: 指定的channel
~~~
number: channel绑定到一个已经打开的fd。  
nil: 空白状态的channel，一般接下来connect。
~~~
*rbufsiz*: channel的读取缓存大小。  
*ctx*: ssl.context。如果未提供则取sslstream_channel.ctx。

##### *返回值*
*ch*: 创建好的table。
~~~
此table可复用但：
    避免使用'ch_'为前缀的字段
    禁止再设置metatable
ch有两个字段可见
    ch_fd     
    ch_state  >0 正常， ==0 EOF， <0 错误
~~~

### connect
-----
同stream_channel.connect

### read
-----
同stream_channel.read

### write
-----
同stream_channel.write

### handshake
-----
err:number = tasklet.sslstream_channel.handshake(self:table[, timedout_sec:number])

_**描述**_: ssl握手。

##### *参数*
*timedout_esc*: 超时时间。

##### *返回值*
*err*: 错误码。

### close
-----
同stream_channel.close

## tasklet.service

### 简介

service用于用于将一个软件模块抽象为服务，其他模块向其提交服务请求并获得应答。

通过require 'tasklet.service'来加载，接口在tasklet名字空间中。

### create_service
-----
svc:table = create_service(name:string, handler:function)

_**描述**_:  创建一个服务

##### *参数*
*name*: 名称。  
*handler*: 处理服务的函数，原型为 resp:any, err:number = handler(svc:table, req:any)。
示例：
1. double服务，即resp = req * 2
```lua
tasklet.create_service('double', function (svc, req)
    if type(req) ~= 'number' then
        return nil, errno.EINVAL
    else
        return req * 2, 0
    end
end)
```

2. redis-get服务（这里只是一个粗略的通信过程且未考虑各种错误）。
```lua
local ch = tasklet.stream_channel.new()
local buf = buffer.new()
ch:connect('127.0.0.1', 6379)
tasklet.create_service('redis-get', function (svc, req)
    buf:rewind():putstr('*2\r\n$3\r\nGET\r\n$', #req, '\r\n', req, '\r\n')
    ch:write(buf)
    local len, val = ch:read(), nil
    len = len and len:match('%$(%-?%d+)')
    if len then
        len = tonumber(len)
        if len >= 0 then
            val = len > 0 and ch:read(len):str() or ''
            ch:read(2)  -- skip '\r\n'
        end
    end
    return val, 0   
end)
```

##### *返回值*
*svc*: 服务对象。此table可以复用但注意避免使用'svc_'前缀的字段。

### create_multi_service
-----
svc:table = create_multi_service(name:string, handler:function, max_num:number, interval:number)

_**描述**_:  创建一个服务。该服务可以将多个请求合并进行处理。

##### *参数*
*name*: 名称。  
*handler*: 处理服务的函数，原型为 handler(reqarr:table, resparr:table, errarr:table, num:number)。

示例：
1. double
```lua
tasklet.create_multi_service('doube', function (reqarr, resparr, errarr, num)
    for i = 1, num do
        local req = reqarr[i]
        if type(req) == 'number' then
            resparr[i] = req * 2
        else
            errarr[i] = errno.EINVAL
        end
    end
end, 20, 0.1)
```

2. redis-get
```lua
local ch = tasklet.stream_channel.new()
local buf = buffer.new()
ch:connect('127.0.0.1', 6379)
tasklet.create_multi_service('redis-get', function (reqarr, resparr, errarr, num)
    buf:rewind():putstr('*', num + 1, '\r\n$4\r\nMGET\r\n')
    for i = 1, num do
        local req = reqarr[i]
        buf:putstr('$', #req, '\r\n', req, '\r\n')
    end
    ch:write(buf)
    ch:read()  -- skip '*...\r\n'
    for i = 1, num do
        local len, val = ch:read(), nil
        len = len and len:match('%$(%-?%d+)')
        if len then
            len = tonumber(len)
            if len >= 0 then
                val = len > 0 and ch:read(len):str() or ''
                ch:read(2)  -- skip '\r\n'
            end
        end
        resparr[i] = val
    end
end, 20, 0.1)
```
*max_num*: 最多一次收集的请求数目。  
*interval*: 完成一次收集的时间。超出此时间后即使未收集到`max_num`个请求也必须进行处理。

##### *返回值*
*svc*: 服务对象。此table可以复用但注意避免使用'svc_'前缀的字段。

### request
-----
resp:any, err:number = request(svc:table/string, req:any[, sec:number])

_**描述**_:  向某个服务发出请求。

##### *参数*
*svc*: 服务名称或者服务对象。  
*req*: 请求的内容。  
*sec*: 超时时间。可选。未指定则一直阻塞直到服务提供者做出应答。

##### *返回值*
*resp*: 服务应答。  
*err*: 错误码。


## app

绝大多数service程序都需要以下功能：解析命令行参数，daemonize， 生成pid文件，设置log，
侦听某个unix/tcp套接字便于外部控制。
app提供了一个解决这些问题的框架。

~~~

这里用一个示例来说明。

[spyderj@mycomputer ~]$ vi app-sample.lua
输入以下内容：

local function main()
    local app = require 'app'
    local tasklet = require 'tasklet'
    app.APPNAME = 'app-sample'
    os.exit(app.run({}, function ()
        -- 启动一个task， 该task 30秒钟后操作一个不存在的变量x导致lua异常。
        tasklet.start_task(function () tasklet.sleep(30) x = x + 1 end)

        -- 启动控制服务端，将生成app-sample.sock
        app.start_ctlserver_task({

            -- 添加add命令
            add = function (argv)
                local a, b = tonumber(argv[1]), tonumber(argv[2])
                if not a or not b then
                    return errno.EINVAL
                end
                return 0, a + b
            end
        })
    end))
end
main()

[spyderj@mycomputer ~]$ vi ctl.lua
输入以下内容：

local appctl = require 'appctl'
appctl.APPNAME = 'app-sample'
appctl.dispatch(arg)


[spyderj@mycomputer ~]$ lua app-sample.lua

运行后自动变为守护进程，在/tmp目录下可以看到app-sample.pid， app-sample.log， app-sample.sock
3个文件。
接下来可以运行lua ctl.lua和app-sample进行交互。

[spyderj@mycomputer ~]$ lua ctl.lua ping
pong
[spyderj@mycomputer ~]$ lua ctl.lua add 1 1
2
[spyderj@mycomputer ~]$ lua ctl.lua add a 1
[ERROR: Invalid argument]
[spyderj@mycomputer ~]$ lua ctl.lua sub 2 1
[ERROR: Function not implemented]

30秒后app-sample由于lua异常退出。 此时/tmp目录下app-sample.pid已被删除，与此同时出现
app-sample.death文件记录了lua栈信息。

~~~

### APPNAME
-----
载入app.lua后务必设置其APPNAME字段。app使用此字段命令相关文件。

### start_ctlserver_task
-----
ch_server:table, server_task:table = start_ctlserver_task(commands:table[, path:string])

_**描述**_: 创建控制服务器

##### *参数*
*commands*: 命令表。
~~~

app实现了如下内置命令
    ping  返回'pong'
    logcapture  劫持日志，将日志的输出路径改成指定的路径(argv[1])，并设置日志级别为argv[2]/debug
    logrelease  取消劫持
    logreopen   重新打开日志文件，可用于日志文件切割
    eval  执行argv[1]
内置命令总是被启用，除非显式指定为false。

自定义命令的原型如下：
err:number, ret:nil/number/string/table = cmd_handler(argv:table)
argv为参数数组。当调用'lua ctl.lua add 1 1'时，argv值为{'1', '1'}。

~~~
*path*: unix套接字文件路径。可选。如果未提供则app自动使用/tmp/[APPNAME].sock。

##### *返回值*  
*ch_server*: streamserver_channel对象。一般可忽略。  
*server_task*: task对象。一般可忽略。


### run
-----
exitcode:number = run(opts:table[, cb_preloop:function])

_**描述**_: 启动应用主循环。

##### *参数*

*opts*: 解析过的命令行选项。
~~~
一般是getopt的返回值，app约定以下指示符
    -d --debug 调试模式，日志输出到控制台，日志级别debug，前台运行，并设置app.DEBUG=true
    -f --foregroud 前台运行，不daemonize
    -o --logpath
    -l --loglevel
    --logflimit 日志文件最大尺寸，如"64k"， "1m"
一旦启用-d/--debug则忽略其他指示符
~~~

*cb_preloop*:  初始化完成后，进入事件处理循环之前调用此函数。可选。

##### *返回值*  
*exitcode*: 程序退出码。

### start_tcpserver_task
-----
ch_server:table, server_task:table = start_tcpserver_task(addr:string, port:number, cb:function)

_**描述**_: 启动一个tcp服务器，此服务器在一个单独的task中不断的accept， 一旦accept成功后调用cb。

##### *参数*
*addr*: ip地址。可选。默认为'0.0.0.0'。  
*port*: 端口号。  
*cb*: 函数原型为cb(fd:number, peer_addr:string, peer_port:number)。
注意此函数运行在server的task中，执行了阻塞操作的话会影响server的accept动作。

##### *返回值*  
*ch_server*: streamserver_channel对象。一般可忽略。  
*server_task*: task对象。一般可忽略。

### start_unserver_task
-----
ch_server:table, server_task:table = start_unserver_task(path:string, cb:function)

_**描述**_: 启动一个unix套接字服务器，此服务器在一个单独的task中不断的accept， 一旦accept成功后调用cb。

##### *参数*
*path*: unix套接字的文件路径。  
*cb*: 函数原型为cb(fd:number)。注意此函数运行在server的task中，执行了阻塞操作的话会影响server的accept动作。

##### *返回值*  
*ch_server*: streamserver_channel对象。一般可忽略。  
*server_task*: task对象。一般可忽略。


## appctl

与app的ctlserver对应，实现了核心的控制客户端功能。
一个典型的控制工具客户端如下：
~~~
local appctl = require 'appctl'
appctl.APPNAME = 'myapp'
appctl.HELP = [[
  ping          test whether the app works well
  logcapture    capture the log
  logrelease    release the capture
  logreopen     reopen the log
]]
if #arg > 0 then
	appctl.dispatch(arg)
else
	appctl.interact()
end
~~~

### APPNAME
-----
appctl使用此名称连接到控制服务端。

### HELP
-----
如果第一个参数时help则自动打印此字符串。

### dispatch
-----
diapatch(argv:table)

_**描述**_: 执行一次命令。

### interact
-----
interact()

_**描述**_: 进入交互模式。

## cjson

编解码json。

### encode
-----
str:string = encode(data:any)

_**描述**_: 编码。

##### *参数*
*data*: 任意lua值。

##### *返回值*  
*str*: 字符串。

### decode
-----
data:any = decode(str:string)

_**描述**_: 解码。

##### *参数*
*str*: json字符串。

##### *返回值*  
*data*: 解码后的值。
~~~
若str非法，decode会直接抛出异常。因此更合理的做法是:
local ok, data = pcall(cjson.decode, str)
~~~

### encodeb
-----
encodeb(data:any, buf:buffer)

_**描述**_: 编码。

##### *参数*
*data*: 任意lua值。  
*buf*: 编码后的内容追加到此buffer中。

### decodeb
-----
data:any = decodeb(src:buffer/reader)

_**描述**_: 解码。

##### *参数*
*src*: 存放要解码内容的buffer/reader对象。  
@TODO: 目前要求buffer/reader中的内容必须以'\0'结尾。建议调用decodeb之前先buf:putb(0)。

##### *返回值*  
*data*: 解码后的值。
~~~
若源内容非法，decodeb会直接抛出异常。因此更合理的做法是:
local ok, data = pcall(cjson.decodeb, src)
~~~

## zlib

封装zlib。提供了对数据的压缩和解压。

### 常量
-----
* MAX_WBITS
* Z_NEED_DICT
* Z_STREAM_ERROR
* Z_DATA_ERROR
* Z_MEM_ERROR
* Z_BUF_ERROR
* Z_VERSION_ERROR

### inflate_init
-----
zstream:userdata, err:number = inflate_init([wbits:number])

_**描述**_: 解压初始化。

##### *参数*
*wbits*: 默认MAX_WBITS + 16。

##### *返回值*  
*zstream*: 不透明的解压上下文对象。失败则为nil。  
*err*: 错误码。  

### inflate
-----
err:number = inflate(zstream:userdata, input:buffer/reader, output:buffer[, flush:number])

_**描述**_: 执行一次解压。

##### *参数*
*zstream*: 解压上下文。  
*input*: 源数据。  
*output*: 解压后追加到此buffer。  
*flush*: 是否还有后续解压操作。可选。默认1（有）。  

##### *返回值*  
*err*: 错误码。

### inflate_end
-----
inflate_end(zstream:userdata)

_**描述**_: 释放解压上下文。

##### *参数*
*zstream*: 解压上下文。

### deflate_init
-----
zstream:userdata, err:number = deflate_init([wbits:number[, level:number[, memlevel:number]]])

_**描述**_: 压缩初始化。

##### *参数*
*wbits*: 默认MAX_WBITS + 16。  
*level*: 压缩等级。默认zlib的Z_DEFAULT_COMPRESSION。  
*memlevel*: 内存等级。默认2。  

##### *返回值*  
*zstream*: 不透明的压缩上下文对象。失败则为nil。  
*err*: 错误码。

### deflate
-----
err:number = deflate(zstream:userdata, input:buffer/reader, output:buffer[, flush:number])

_**描述**_: 执行一次压缩。

##### *参数*
*zstream*: 压缩上下文。  
*input*: 源数据。  
*output*: 压缩后的数据写到此buffer。  
*flush*: 是否还有后续压缩操作。可选。默认1（有）。

##### *返回值*  
*err*: 错误码。

### deflate_end
-----
deflate_end(zstream:userdata)

_**描述**_: 释放压缩上下文。

##### *参数*
*zstream*: 压缩上下文。


### compress
-----
err:number = compress(input:buffer[, output:buffer])

_**描述**_: 压缩。这是对deflate_init/deflate/deflate_end的简单封装。

##### *参数*
*input*: 要压缩的数据。  
*output*: 存放压缩后的数据。可选。若未提供则放回input（此时要求input不能是tmpbuf）

##### *返回值*  
*err*: 错误码。

### uncompress
-----
err:number = uncompress(input:buffer[, output:buffer])

_**描述**_: 解压。这是对inflate_init/inflate/inflate_end的简单封装。

##### *参数*
*input*: 要解压的数据。  
*output*: 存放解压后的数据。可选。若未提供则放回input（此时要求input不能是tmpbuf）

##### *返回值*  
*err*: 错误码。

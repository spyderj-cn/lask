
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#define pid2num(pid)					((lua_Number)(pid))

int os_getnread(int fd, size_t *nread)
{
	int value = 0;
	int err = ioctl(fd, FIONREAD, &value);
	if (err < 0)
		err = errno;
	else
		*nread = (size_t)value;
	return err;
}

int os_implread(int fd, Buffer *buffer, ssize_t nreq, size_t *ndone,
				ssize_t (*read_cb)(int, void*, size_t, void*), void* ud)
{
	char tmp[1024];
	ssize_t nacc = 0;
	int err = 0;

	if (ndone != NULL)
		*ndone = 0;

	if (nreq == 0) {
		ssize_t ret = read_cb(fd, NULL, 0, ud);
		return ret == 0 ? 0 : errno;
	}

	while (nacc < nreq || nreq < 0) {
		size_t num = sizeof(tmp);
		ssize_t ret;

		if (num > (size_t)(nreq - nacc))
			num = (size_t)(nreq - nacc);

		ret = read_cb(fd, tmp, num, ud);
		if (ret > 0) {
			nacc += (size_t)ret;
			buffer_push(buffer, tmp, ret);
			if ((size_t)ret < num)
				break;
		} else if (ret == 0) {
			break;
		} else {
			err = errno;
			if (err == EINTR)
				continue;
			else {
				if (err == EWOULDBLOCK || err == EAGAIN)
					err = 0;
				break;
			}
		}
	}

	if (ndone != NULL)
		*ndone = (size_t)nacc;

	return err;
}

static ssize_t read_cb(int fd, void *mem, size_t memsiz, void* ud)
{
	unused(ud);
	return read(fd, mem, memsiz);
}

int os_read(int fd, Buffer *buffer, ssize_t nreq, size_t *ndone)
{
	return os_implread(fd, buffer, nreq, ndone, read_cb, NULL);
}

int os_implwrite(int fd, const uint8 *mem, size_t nreq, size_t *ndone,
				ssize_t (*write_cb)(int, const void*, size_t, void*), void* ud)
{
	size_t total = 0;
	int err = 0;

	if (ndone != NULL)
		*ndone = 0;

	while (total < nreq) {
		ssize_t ret = write_cb(fd, mem, nreq - total, ud);
		err = 0;
		if (ret > 0) {
			total += (size_t)ret;
			mem += ret;
		} else if (ret == 0) {
			break;
		} else {
			err = errno;
			if (err == EINTR)
				continue;
			else {
				if (err == EWOULDBLOCK || err == EAGAIN)
					err = 0;
				break;
			}
		}
	}
	if (err == 0 && ndone != NULL)
		*ndone = total;

	return err;
}

static ssize_t write_cb(int fd, const void* mem, size_t memsiz, void *ud)
{
	unused(ud);
	return write(fd, mem, memsiz);
}

int os_write(int fd, const uint8 *mem, size_t nreq, size_t *ndone)
{
	return os_implwrite(fd, mem, nreq, ndone, write_cb, NULL);
}

/*
** fd, err = os.open(path, flags, mode)
*/
static int los_open(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int flags = (int)luaL_checkinteger(L, 2);
	mode_t mode = 0;
	int fd;

	if (lua_gettop(L) >= 3)
		mode = (mode_t)luaL_checkinteger(L, 2);

	fd = open(path, flags, mode);
	lua_pushinteger(L, fd);
	lua_pushinteger(L, fd < 0 ? errno : 0);
	return 2;
}

/*
** fd, err = os.creat(path[, mode])
*/
static int los_creat(lua_State *L)
{
	int fd = creat(luaL_checkstring(L, 1), (mode_t)luaL_checkinteger(L, 2));
	lua_pushinteger(L, fd);
	lua_pushinteger(L, fd < 0 ? errno : 0);
	return 2;
}

/*
** str, err = os.read(fd, n=nil)
** read n bytes or all available(if n is nil) from fd and return them as a string
** will never block even if fd is not nonblocking when n is nil
**
** (str, err):
** 	(str, 0)  		succeed
**  (nil, 0)  		no data available (together with readable-event means fd is half-closed).
** 	(nil, non-0)  	error on fd
**
** note that EINTR/EAGAIN/EWOUDLBLOCK are filtered
*/
static int los_read(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	Buffer buffer;
	size_t ndone = 0;
	ssize_t nreq = (ssize_t)luaL_optinteger(L, 2, -1);
	int err;

	buffer_init(&buffer, 0);
	err = os_read(fd, &buffer, nreq, &ndone);
	if (err == 0 && ndone > 0)
		lua_pushlstring(L, (const char*)buffer.data, buffer.datasiz);
	else
		lua_pushnil(L);

	lua_pushinteger(L, err);
	buffer_finalize(&buffer);
	return 2;
}

/*
** nread, err = os.readb(fd, buffer, nbytes=-1)
** read n bytes or all available(if n is nil) from fd and push them into buffer
** will never block even if fd is not nonblocking when n is nil
**
** return number of bytes read plus the error code
** (nread, err):
** 	(>0, 0)  		succeed
**  (==0, 0)  		no data available (together with readable-event means fd is half-closed).
** 	(==0, non-0)  	error on fd
**
** note that EINTR/EAGAIN/EWOUDLBLOCK are filtered
*/
static int los_readb(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	Buffer *buffer = buffer_lcheck(L, 2);
	ssize_t nreq = luaL_optinteger(L, 3, -1);
	size_t ndone = 0;
	int err = os_read(fd, buffer, nreq, &ndone);
	lua_pushinteger(L, ndone);
	lua_pushinteger(L, err);
	return 2;
}

/*
** nbytes = os.getnread(fd)
** get number of bytes that are readable
*/
static int los_getnread(lua_State *L)
{
	size_t n;

	if (os_getnread((int)luaL_checkinteger(L, 1), &n) != 0)
		n = 0;

	lua_pushinteger(L, n);
	return 1;
}

/*
** filepath, err = os.readlinke(filepath)
*/
static int los_readlink(lua_State *L)
{
	char buf[PATH_MAX];
	const char *filepath = luaL_checkstring(L, 1);
	ssize_t n = readlink(filepath, buf, sizeof(buf));
	if (n >= 0) {
		lua_pushlstring(L, buf, (size_t)n);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	return 2;
}

/*
** nwrite, err = os.write(fd, ...)
** write a list of values(strings or things with tostring ability)
**
** return number of bytes written, plus the error code
**
** note that EINTR/EAGAIN/EWOUDLBLOCK are filtered
*/
static int los_write(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int top = lua_gettop(L);
	size_t ndone;
	int err;

	if (top == 2) {
		size_t nreq = 0;
		const char *str = luaL_checklstring(L, 2, &nreq);
		err = os_write(fd, (const uint8*)str, nreq, &ndone);
	} else {
		Buffer *buffer;

		lua_pushlightuserdata(L, los_write);
		lua_gettable(L, LUA_REGISTRYINDEX);
		if (lua_isuserdata(L, -1)) {
			buffer = (Buffer*)lua_touserdata(L, -1);
			buffer_rewind(buffer);
		} else {
			lua_pushlightuserdata(L, los_write);
			buffer = (Buffer*)lua_newuserdata(L, sizeof(Buffer));
			lua_settable(L, LUA_REGISTRYINDEX);
			buffer_init(buffer, 0);
		}

		for (int i = 2; i <= top; i++) {
			size_t len = 0;
			const char *str = lua_tolstring(L, i, &len);
			if (str != NULL) {
				buffer_push(buffer, (const uint8*)str, len);
			} else {
				lua_pushfstring(L, "expecting string or number of argument #%d", i);
				lua_error(L);
			}
		}
		err = os_write(fd, buffer->data, buffer->datasiz, &ndone);
	}
	lua_pushinteger(L, ndone);
	lua_pushinteger(L, err);
	return 2;
}

/*
** nwrite, err = os.writeb(fd, buffer/reader, offset=0, length=all)
**
**
** return number of bytes written, plus the error code
**
** note that EINTR/EAGAIN/EWOUDLBLOCK are filtered
*/
static int los_writeb(lua_State *L)
{
	union {
		const Buffer *buffer;
		const Reader *reader;
	}ptr;
	const uint8 *data = NULL;
	size_t datasiz = 0;
	int fd = (int)luaL_checkinteger(L, 1);
	size_t offset = (size_t)luaL_optinteger(L, 3, 0);
	size_t length = 0;
	size_t ndone = 0;
	int err = 0;

	ptr.buffer = (const Buffer*)lua_touserdata(L, 2);
	if (ptr.buffer != NULL) {
		if (ptr.buffer->magic == BUFFER_MAGIC) {
			data = ptr.buffer->data;
			datasiz = ptr.buffer->datasiz;
		} else if (ptr.reader->magic == READER_MAGIC) {
			data = ptr.reader->data;
			datasiz = ptr.reader->datasiz;
		} else {
			ptr.buffer = NULL;
		}
	}
	if (ptr.buffer == NULL) {
		luaL_error(L, "expecting userdata buffer/reader for argument 2");
	}

	length = datasiz;
	if (lua_gettop(L) >= 4)
		length = (size_t)luaL_checkinteger(L, 4);

	if (offset >= datasiz)
		length = 0;
	else if ((offset + length) > datasiz)
		length = (datasiz - offset);

	if (length > 0)
		err = os_write(fd, data + offset, length, &ndone);

	lua_pushinteger(L, ndone);
	lua_pushinteger(L, err);
	return 2;
}

/*
** currpos, err = os.lseek(fd, offset, whence)
*/
static int los_lseek(lua_State *L)
{
	int fd, err = 0;
	off_t offset = 0;
	int whence = (int)luaL_checkinteger(L, 3);

	if (whence != SEEK_SET
		&& whence != SEEK_CUR
		&& whence != SEEK_END) {
		err = EINVAL;
	} else {
		fd = (int)luaL_checkinteger(L, 1);
		offset = (off_t)luaL_checkinteger(L, 2);
		offset = lseek(fd, offset, whence);
		if (offset < 0) {
			err = errno;
			offset = 0;
		}
	}
	lua_pushinteger(L, offset);
	lua_pushinteger(L, err);
	return 2;
}

/*
** err = os.fsync(fd)
*/
static int los_fsync(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int err = fsync(fd);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** err = os.fdatasync(fd)
*/
static int los_fdatasync(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int err = fdatasync(fd);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** err = os.truncate(filepath, length)
*/
static int los_truncate(lua_State *L)
{
	const char *filepath = luaL_checkstring(L, 1);
	off_t off = (off_t)luaL_checkinteger(L, 2);
	lua_pushinteger(L, truncate(filepath, off) == 0 ? 0 : errno);
	return 1;
}

/*
** err = os.ftruncate(filepath, length)
*/
static int los_ftruncate(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	off_t off = (off_t)luaL_checkinteger(L, 2);
	lua_pushinteger(L, ftruncate(fd, off) == 0 ? 0 : errno);
	return 1;
}

/*
** new_fd, err = os.dup(fd)
*/
static int los_dup(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int new_fd = dup(fd);
	lua_pushinteger(L, new_fd);
	lua_pushinteger(L, new_fd >= 0 ? 0 : errno);
	return 2;
}

/*
** new_fd, err = os.dup2(fd, fd2)
*/
static int los_dup2(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int fd2 = (int)luaL_checkinteger(L, 2);
	int new_fd = dup2(fd, fd2);
	lua_pushinteger(L, new_fd);
	lua_pushinteger(L, new_fd >= 0 ? 0 : errno);
	return 2;
}

/*
** err = os.close(fd)
*/
static int los_close(lua_State *L) {
	int fd = (int)luaL_checkinteger(L, 1);
	int err = close(fd);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** err = os.closerange(minfd=0, maxfd=FD_SETSIZE)
**
** close every fd in the range [minfd, maxfd]
*/
static int los_closerange(lua_State *L)
{
	int minfd = (int)luaL_optinteger(L, 1, 0);
	int maxfd = (int)luaL_optinteger(L, 2, FD_SETSIZE);

	if (minfd < 0)
		minfd = 0;

	if (maxfd >= minfd) {
		for (int i = minfd; i <= maxfd; i++)
			close(i);
	}

	return 0;
}

/*
** os.setnonblock(fd)
*/
static int los_setnonblock(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0) {
		fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	}
	return 0;
}

/*
** os.setcloexec(fd)
*/
static int los_setcloexec(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int val = fcntl(fd, F_GETFD, 0);
	if (val >= 0) {
		fcntl(fd, F_SETFD, val | FD_CLOEXEC);
	}
	return 0;
}

/*
** true/false = os.isatty(fd)
*/
static int los_isatty(lua_State *L)
{
	lua_pushboolean(L, isatty((int)luaL_checkinteger(L, 1)));
	return 1;
}

/*
**  err = os.system(cmd)
*/
static int los_system(lua_State *L) {
	int err = system(lua_tostring(L, 1));
	lua_pushinteger(L, err);
	return 1;
}

/*
** pid, err = os.fork()
*/
static int los_fork(lua_State *L)
{
	pid_t pid = fork();
	int err = pid < 0 ? errno : 0;
	lua_pushinteger(L, pid);
	lua_pushinteger(L, err);
	return 2;
}

/*
** pid, err = os.vfork()
*/
static int los_vfork(lua_State *L)
{
	pid_t pid = vfork();
	int err = pid < 0 ? errno : 0;
	lua_pushinteger(L, pid);
	lua_pushinteger(L, err);
	return 2;
}

/*
** rdfd, wrfd, err = os.pipe()
*/
static int los_pipe(lua_State *L)
{
	int fd[2];
	int err = pipe(fd);
	if (err == 0) {
		lua_pushinteger(L, fd[0]);
		lua_pushinteger(L, fd[1]);
		lua_pushinteger(L, 0);
	} else {
		lua_pushinteger(L, -1);
		lua_pushinteger(L, -1);
		lua_pushinteger(L, errno);
	}
	return 3;
}

/*
** pid = os.setsid()
**
** pid will be -1 if failed, the only reason is permission-denied
*/
static int los_setsid(lua_State *L)
{
	lua_pushinteger(L, setsid());
	return 1;
}

/*
**  pid = os.getpid()
*/
static int los_getpid(lua_State *L)
{
	lua_pushinteger(L, (int)getpid());
	return 1;
}

/*
** pid = os.getppid()
*/
static int los_getppid(lua_State *L)
{
	lua_pushinteger(L, (int)getppid());
	return 1;
}

/*
** uid = os.getppid()
*/
static int los_getuid(lua_State *L)
{
	lua_pushinteger(L, (int)getuid());
	return 1;
}

/*
** uid = os.geteuid()
*/
static int los_geteuid(lua_State *L)
{
	lua_pushinteger(L, (int)geteuid());
	return 1;
}

/*
** gid = os.getgid()
*/
static int los_getgid(lua_State *L)
{
	lua_pushinteger(L, (int)getgid());
	return 1;
}

/*
** gid = os.getegid()
*/
static int los_getegid(lua_State *L)
{
	lua_pushinteger(L, (int)getegid());
	return 1;
}

/*
** err = os.setuid(uid)
*/
static int los_setuid(lua_State *L)
{
	uid_t uid = (uid_t)luaL_checkinteger(L, 1);
	int err = setuid(uid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = os.seteuid(uid)
*/
static int los_seteuid(lua_State *L)
{
	uid_t uid = (uid_t)luaL_checkinteger(L, 1);
	int err = seteuid(uid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = os.setgid(gid)
*/
static int los_setgid(lua_State *L)
{
	gid_t gid = (gid_t)luaL_checkinteger(L, 1);
	int err = setgid(gid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = os.setegid(gid)
*/
static int los_setegid(lua_State *L)
{
	gid_t gid = (gid_t)luaL_checkinteger(L, 1);
	int err = setegid(gid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** pid, stat/err = os.wait()
*/
static int los_wait(lua_State *L)
{
	int stat;
	pid_t pid = wait(&stat);
	if (pid >= 0) {
		lua_pushinteger(L, (int)pid);
		lua_pushinteger(L, stat);
	} else {
		lua_pushinteger(L, -1);
		lua_pushinteger(L, errno);
	}
	return 2;
}

/*
** pid, stat/err = os.waitpid(pid, flags)
*/
static int los_waitpid(lua_State *L)
{
	pid_t pid = (pid_t)luaL_checkinteger(L, 1);
	int flags = (int)luaL_optinteger(L, 2, 0);
	int status = 0;

	pid = waitpid(pid, &status, flags);
	if (pid >= 0) {
		lua_pushinteger(L, (int)pid);
		lua_pushinteger(L, status);
	} else {
		lua_pushinteger(L, -1);
		lua_pushinteger(L, errno);
	}
	return 2;
}

/*
** value = os.WEXITSTATUS(value)
*/
static int los_WEXITSTATUS(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WEXITSTATUS(status));
	return 1;
}

#ifdef WIFCONTINUED
/*
** value = os.WIFCONTINUED(value)
*/
static int los_WIFCONTINUED(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WIFCONTINUED(status));
	return 1;
}
#endif

/*
** value = os.WIFEXITED(value)
*/
static int los_WIFEXITED(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WIFEXITED(status));
	return 1;
}

/*
** value = os.WIFSIGNALED(value)
*/
static int los_WIFSIGNALED(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WIFSIGNALED(status));
	return 1;
}

/*
** value = os.WIFSTOPPED(value)
*/
static int los_WIFSTOPPED(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WIFSTOPPED(status));
	return 1;
}

/*
** value = os.WSTOPSIG(value)
*/
static int los_WSTOPSIG(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WSTOPSIG(status));
	return 1;
}

/*
** value = os.WTERMSIG(value)
*/
static int los_WTERMSIG(lua_State *L)
{
	int status = (int)luaL_checkinteger(L, 1);
	lua_pushinteger(L, WTERMSIG(status));
	return 1;
}

/*
** err = execl(path, arg1, arg2, ...)
*/
static int los_execl(lua_State *L)
{
	int top = lua_gettop(L);
	if (top > 32) {
		luaL_error(L, "too many arguments, now maximum is 31");
	} else {
		const char *path = luaL_checkstring(L, 1);
		int argc = 0;
		char *argv[32];
		while (lua_isstring(L, argc + 2)) {
			argv[argc] = (char*)lua_tostring(L, argc + 2);
			argc++;
		}
		argv[argc] = NULL;
		execv(path, argv);
		lua_pushinteger(L, errno);
	}
	return 1;
}

static const EnumReg enums[] = {
	LENUM(O_RDONLY),
	LENUM(O_WRONLY),
	LENUM(O_RDWR),
	LENUM(O_APPEND),
	LENUM(O_CREAT),
	LENUM(O_EXCL),
	LENUM(O_NONBLOCK),
	LENUM(O_DSYNC),
	LENUM(O_ASYNC),
	LENUM(O_RSYNC),
	LENUM(O_SYNC),
	LENUM(O_TRUNC),
#ifdef O_CLOEXEC
	LENUM(O_CLOEXEC),
#endif
	LENUM(SEEK_CUR),
	LENUM(SEEK_SET),
	LENUM(SEEK_END),
	LENUM(F_RDLCK),
	LENUM(F_WRLCK),
	LENUM(F_UNLCK),
	LENUM(WNOHANG),
	LENUM(WUNTRACED),
	LENUM_NULL
};

static const luaL_Reg funcs[] = {
	{"open", los_open},
	{"creat", los_creat},
	{"getnread", los_getnread},
	{"read", los_read},
	{"readb", los_readb},
	{"write", los_write},
	{"writeb", los_writeb},
	{"readlink", los_readlink},
	{"lseek", los_lseek},
	{"fsync", los_fsync},
	{"fdatasync", los_fdatasync},
	{"close", los_close},
	{"closerange", los_closerange},
	{"ftruncate", los_ftruncate},
	{"truncate", los_truncate},
	{"dup", los_dup},
	{"dup2", los_dup2},
	{"pipe", los_pipe},
	{"setnonblock", los_setnonblock},
	{"setcloexec", los_setcloexec},
	{"isatty", los_isatty},

	{"setsid", los_setsid},
	{"getpid", los_getpid},
	{"getppid", los_getppid},
	{"getuid", los_getuid},
	{"geteuid", los_geteuid},
	{"getgid", los_getgid},
	{"getegid", los_getegid},
	{"setuid", los_setuid},
	{"seteuid", los_seteuid},
	{"setgid", los_setgid},
	{"setegid", los_setegid},

	{"execl", los_execl},
	{"system", los_system},
	{"fork", los_fork},
	{"vfork", los_vfork},
	{"wait", los_wait},
	{"waitpid", los_waitpid},
	{"WEXITSTATUS", los_WEXITSTATUS},
#ifdef WIFCONTINUED
	{"WIFCONTINUED", los_WIFCONTINUED},
#endif
	{"WIFEXITED", los_WIFEXITED},
	{"WIFSIGNALED", los_WIFSIGNALED},
	{"WIFSTOPPED", los_WIFSTOPPED},
	{"WSTOPSIG", los_WSTOPSIG},
	{"WTERMSIG", los_WTERMSIG},

	{NULL, NULL}
};

int l_openos(lua_State *L)
{
	l_register_lib(L, "os", funcs, enums);
	return 0;
}

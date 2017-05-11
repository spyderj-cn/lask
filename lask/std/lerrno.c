
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <errno.h>

static const EnumReg enums[] = {
	LENUM(E2BIG),
	LENUM(EACCES),
	LENUM(EADDRINUSE),
	LENUM(EADDRNOTAVAIL),
	LENUM(EAFNOSUPPORT),
	LENUM(EAGAIN),
	LENUM(EALREADY),
	LENUM(EBADF),
	LENUM(EBADMSG),
	LENUM(EBUSY),
	LENUM(ECANCELED),
	LENUM(ECHILD),
	LENUM(ECONNABORTED),
	LENUM(ECONNREFUSED),
	LENUM(ECONNRESET),
	LENUM(EDEADLK),
	LENUM(EDESTADDRREQ),
	LENUM(EDOM),
	LENUM(EDQUOT),
	LENUM(EEXIST),
	LENUM(EFAULT),
	LENUM(EFBIG),
	LENUM(EHOSTUNREACH),
	LENUM(EIDRM),
	LENUM(EILSEQ),
	LENUM(EINPROGRESS),
	LENUM(EINTR),
	LENUM(EINVAL),
	LENUM(EIO),
	LENUM(EISCONN),
	LENUM(EISDIR),
	LENUM(ELOOP),
	LENUM(EMFILE),
	LENUM(EMLINK),
	LENUM(EMSGSIZE),
	LENUM(EMULTIHOP),
	LENUM(ENAMETOOLONG),
	LENUM(ENETDOWN),
	LENUM(ENETRESET),
	LENUM(ENETUNREACH),
	LENUM(ENFILE),
	LENUM(ENOBUFS),
	LENUM(ENODATA),
	LENUM(ENODEV),
	LENUM(ENOENT),
	LENUM(ENOEXEC),
	LENUM(ENOLCK),
	LENUM(ENOLINK),
	LENUM(ENOMEM),
	LENUM(ENOMSG),
	LENUM(ENOPROTOOPT),
	LENUM(ENOSPC),
	LENUM(ENOSR),
	LENUM(ENOSTR),
	LENUM(ENOSYS),
	LENUM(ENOTCONN),
	LENUM(ENOTDIR),
	LENUM(ENOTEMPTY),
	LENUM(ENOTSOCK),
	LENUM(ENOTSUP),
	LENUM(ENOTTY),
	LENUM(ENXIO),
	LENUM(EOPNOTSUPP),
	LENUM(EOVERFLOW),
	LENUM(EPERM),
	LENUM(EPIPE),
	LENUM(EPROTO),
	LENUM(EPROTONOSUPPORT),
	LENUM(EPROTOTYPE),
	LENUM(ERANGE),
	LENUM(EROFS),
	LENUM(ESPIPE),
	LENUM(ESRCH),
	LENUM(ESTALE),
	LENUM(ETIME),
	LENUM(ETIMEDOUT),
	LENUM(ETXTBSY),
	LENUM(EWOULDBLOCK),
	LENUM(EXDEV),
	LENUM_NULL
};

typedef struct _Desc {
	int err;
	const char *text;
}Desc;
#define DESC(err, desc)		{err, desc}

static const Desc descs[] = {
	DESC(ESUCCEED, "Succeed"),
	DESC(E2BIG, "Argument list too long"),
	DESC(EACCES, "Permission denied"),
	DESC(EADDRINUSE, "Address in use"),
	DESC(EAFNOSUPPORT, "Address family not supported"),
	DESC(EAGAIN, "Resource temporarily unavailable"),
	DESC(EALREADY, "Connection already in progress"),
	DESC(EBADF, "Bad file descriptor"),
	DESC(EBADMSG, "Bad message"),
	DESC(EBUSY, "Resource busy"),
	DESC(ECANCELED, "Operation canceled"),
	DESC(ECHILD, "No child process"),
	DESC(ECONNABORTED, "Connection aborted"),
	DESC(ECONNREFUSED, "Connection refused"),
	DESC(ECONNRESET, "Connection reset"),
	DESC(EDEADLK, "Resource deadlock would occur"),
	DESC(EDESTADDRREQ, "Destination address required"),
	DESC(EDOM, "Domain error"),
	DESC(EDQUOT, "Reserved"),
	DESC(EEXIST, "File exists"),
	DESC(EFAULT, "Bad address"),
	DESC(EFBIG, "File too large"),
	DESC(EHOSTUNREACH, "Host is unreachable"),
	DESC(EIDRM, "Identifier removed"),
	DESC(EILSEQ, "Illegal byte sequence"),
	DESC(EINPROGRESS, "Operation in progress"),
	DESC(EINTR, "Interrupted function call"),
	DESC(EINVAL, "Invalid argument"),
	DESC(EIO, "Input/output error"),
	DESC(EISCONN, "Socket is connected"),
	DESC(EISDIR, "Is a directory"),
	DESC(ELOOP, "Symbolic link loop"),
	DESC(EMFILE, "Too many open files"),
	DESC(EMLINK, "Too many links"),
	DESC(EMSGSIZE, "Message too large"),
	DESC(EMULTIHOP, "Reserved"),
	DESC(ENAMETOOLONG, "Filename too long"),
	DESC(ENETDOWN, "Network is down"),
	DESC(ENETRESET, "The connection was aborted by the network"),
	DESC(ENETUNREACH, "Network unreachable"),
	DESC(ENFILE, "Too many files open in system"),
	DESC(ENOBUFS, "No buffer space available"),
	DESC(ENODATA, "No message available"),
	DESC(ENODEV, "No such device"),
	DESC(ENOENT, "No such file or directory"),
	DESC(ENOEXEC, "Executable file format error"),
	DESC(ENOLCK, "No locks available"),
	DESC(ENOLINK, "Reserved"),
	DESC(ENOMEM, "Not enough memory"),
	DESC(ENOMSG, "No message of the desired type"),
	DESC(ENOPROTOOPT, "Protocol not available"),
	DESC(ENOSPC, "No space left on a device"),
	DESC(ENOSR, "No STREAM resources"),
	DESC(ENOSTR, "Not a STREAM"),
	DESC(ENOSYS, "Function not implemented"),
	DESC(ENOTCONN, "Socket not connected"),
	DESC(ENOTDIR, "Not a directory"),
	DESC(ENOTEMPTY, "Directory not empty"),
	DESC(ENOTSOCK, "Not a socket"),
	DESC(ENOTSUP, "Not supported"),
	DESC(ENOTTY, "Inappropriate I/O control operation"),
	DESC(ENXIO, "No such device or address"),
	DESC(EOPNOTSUPP, "Operation not supported on socket"),
	DESC(EOVERFLOW, "Value too large to be stored in data type"),
	DESC(EPERM, "Operation not permitted"),
	DESC(EPIPE, "Broken pipe"),
	DESC(EPROTO, "Protocol error"),
	DESC(EPROTONOSUPPORT, "Protocol not supported"),
	DESC(EPROTOTYPE, "Protocol wrong type for socket"),
	DESC(ERANGE, "Result too large or too small"),
	DESC(EROFS, "Read-only file system"),
	DESC(ESPIPE, "Invalid seek"),
	DESC(ESRCH, "No such process"),
	DESC(ESTALE, "Reserved"),
	DESC(ETIME, "STERAM ioctl() timeout"),
	DESC(ETIMEDOUT, "Connection timed out"),
	DESC(ETXTBSY, "Text file busy"),
	DESC(EWOULDBLOCK, "Operation would block"),
	DESC(EXDEV, "Improper link"),

	{0, NULL}
};

/*
** err = errno.errno()
*/
static int lerrno_errno(lua_State *L)
{
	lua_pushinteger(L, errno);
	return 1;
}

/*
** str = errno.strerror(err)
*/
static int lerrno_strerror(lua_State *L)
{
	int err = (int)luaL_optinteger(L, 1, -1);
	const Desc *p = descs;
	const char *text = NULL;
	char unknown[32];
	
	if (err < 0)
		err = errno;

	while (p->text != NULL) {
		if (p->err == err) {
			text = p->text;
			break;
		}
		p++;
	}

	if (text == NULL) {
		snprintf(unknown, sizeof(unknown), "Unknown error (%d)", err);
		text = unknown;
	}

	lua_pushstring(L, text);
	return 1;
}

static const luaL_Reg funcs[]  ={
	{"errno", lerrno_errno},
	{"strerror", lerrno_strerror},
	{NULL, NULL}
};

int l_openerrno(lua_State *L)
{
	l_register_lib(L, "errno", funcs, enums);
	return 0;
}

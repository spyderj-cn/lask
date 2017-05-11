
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <glob.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#define FS_DIR_META			"meta(fs.dir)"
#define FS_GLOB_META		"meta(fs.glob)"

#define isdot(p)			(p[0] == '.' && p[1] == '\0')
#define is2dot(p)			(p[0] == '.' && p[1] == '.' && p[2] == '\0')

typedef struct {
	glob_t gl;
	size_t pos;
	int	freed;
} glob_context_t;

static int lfs_dir_next(lua_State *L)
{
	DIR **pdir = (DIR**)luaL_checkudata(L, 1, FS_DIR_META);
	DIR *dir = *pdir;
	struct dirent *dp;

	while ((dp = readdir(dir)) != NULL) {
		const char *name = dp->d_name;
		if (!isdot(name) && !is2dot(name)) {
			lua_pushstring(L, dp->d_name);
			return 1;
		}
	}
	closedir(dir);
	*pdir = NULL;
	return 0;
}

static int lfs_dir_close(lua_State *L)
{
	DIR **pdir = (DIR**)luaL_checkudata(L, 1, FS_DIR_META);
	if (*pdir != NULL) {
		closedir(*pdir);
		*pdir = NULL;
	}
	return 0;
}

/*
** for entry in fs.dir(path) do ... end
*/
static int lfs_dir(lua_State *L)
{
	const char *path = luaL_optstring(L, 1, ".");
	DIR *dir = opendir(path);
	if (dir == NULL) {
		lua_pushnil(L);
		lua_pushnil(L);
	} else {
		lua_pushcfunction(L, lfs_dir_next);
		DIR **pdir = (DIR**)lua_newuserdata(L, sizeof(DIR*));
		l_setmetatable(L, -1, FS_DIR_META);
		*pdir = dir;
	}
	return 2;
}

/*
** filelist, err = fs.listdir(path)
*/
static int lfs_listdir(lua_State *L)
{
	const char *path = luaL_optstring(L, 1, ".");
	DIR *dir = opendir(path);
	struct dirent *dp;

	if (dir != NULL) {
		int idx = 1;
		lua_newtable(L);
		while ((dp = readdir(dir)) != NULL){
			const char *name = dp->d_name;
			if (!isdot(name) && !is2dot(name)) {
				lua_pushinteger(L, idx);
				lua_pushstring(L, name);
				lua_settable(L, -3);
				idx++;
			}
		}
		closedir(dir);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	return 2;
}

static int lfs_glob_next(lua_State *L)
{
	glob_context_t *ctx = lua_touserdata(L, lua_upvalueindex(1));
	if (!ctx->freed && ctx->pos < ctx->gl.gl_pathc) {
		lua_pushstring(L, ctx->gl.gl_pathv[(ctx->pos)++]);
	} else {
		if (!ctx->freed) {
			globfree(&ctx->gl);
			ctx->freed = 1;
		}
		lua_pushnil(L);
	}
	return 1;
}

static int lfs_glob_gc(lua_State *L)
{
	glob_context_t *ctx = lua_touserdata(L, 1);
	if (ctx && !ctx->freed) {
		ctx->freed = 1;
		globfree(&ctx->gl);
	}
	return 0;
}

static int iternil(lua_State *L)
{
	unused(L);
	return 0;
}

/*
** for path in fs.glob(...) do ... end
*/
static int lfs_glob(lua_State *L)
{
	 const char *pattern = luaL_optstring(L, 1, "*");
	 glob_context_t *globres = lua_newuserdata(L, sizeof(glob_context_t));
	 int globstat = 0;

	 globres->pos = 0;
	 globres->freed = 0;

	 globstat = glob(pattern, 0, NULL, &globres->gl);
	 if (globstat != 0) {
		 lua_pushcfunction(L, iternil);
		 lua_pushinteger(L, 0);
	 } else {
		 luaL_getmetatable(L, FS_GLOB_META);
		 lua_setmetatable(L, -2);
		 lua_pushcclosure(L, lfs_glob_next, 1);
		 lua_pushinteger(L, globres->gl.gl_pathc);
	 }
	 return 2;
}

/*
** errno = fs.chdir(path)
*/
static int lfs_chdir(lua_State *L)
{
	const char *dir = lua_tostring(L, 1);
	int err = chdir(dir);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** path = fs.getcwd()
*/
static int lfs_getcwd(lua_State *L)
{
	char buf[PATH_MAX + 1];
	lua_pushstring(L, getcwd(buf, PATH_MAX));
	return 1;
}

/*
** err = fs.mkdir(path)
*/
static int lfs_mkdir(lua_State *L)
{
	int err;
	mode_t mode;
	if (lua_gettop(L) > 1) {
		mode = (mode_t)luaL_checkinteger(L, 2);
	} else {
		mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	}
	err = mkdir(lua_tostring(L, 1), mode);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

static int makedirs(const char *path, mode_t mode)
{
	char buf[PATH_MAX + 1];
	size_t len = strlen(path);

	len = MIN(PATH_MAX, len);
	memcpy(buf, path, len);
	buf[len] = 0;
	if (buf[len - 1] == '/')
		buf[len - 1] = 0;

	if (access(path, F_OK) == 0) {
		return 0;
	} else {
		char *dir = dirname(buf);
		int err = makedirs(dir, mode);
		if (err != 0)
			return err;
		else
			return mkdir(path, mode) == 0 ? 0 : errno;
	}
}

/*
** err = fs.mkdir_p(path)
*/
static int lfs_mkdir_p(lua_State *L)
{
	const char *dir = luaL_checkstring(L, 1);
	mode_t mode;
	if (lua_gettop(L) > 1) {
		mode = (mode_t)luaL_checkinteger(L, 2);
	} else {
		mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	}
	int err = makedirs(dir, mode);
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.rmdir(path)
*/
static int lfs_rmdir(lua_State *L)
{
	int err = rmdir(luaL_checkstring(L, 1));
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** err = fs.unlink(path)
*/
static int lfs_unlink(lua_State *L)
{
	int err = unlink(luaL_checkstring(L, 1));
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** err = fs.access(path, mode)
*/
static int lfs_access(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int mode = luaL_checkinteger(L, 2);
	int err = access(path, mode);
	lua_pushinteger(L, err ? errno : 0);
	return 1;
}

/*
** name = fs.basename(path)
*/
static int lfs_basename(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	char base[PATH_MAX];
	base[PATH_MAX-1] = 0;
	strncpy(base, path, PATH_MAX - 1);
	lua_pushstring(L, basename(base));
	return 1;
}

/*
** name = fs.dirname(path)
*/
static int lfs_dirname(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	char base[PATH_MAX];
	base[PATH_MAX-1] = 0;
	strncpy(base, path, PATH_MAX-1);
	lua_pushstring(L, dirname(base));
	return 1;
}

/*
** name = fs.realpath(path)
*/
static int lfs_realpath(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	char real[PATH_MAX];

	if (!realpath(path, real)) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, real);
	}
	return 1;
}

static int ustat_retval(lua_State *L, struct stat *st, int err)
{
	if (err != 0) {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	} else {
		int field = lua_tointeger(L, -1);
		lua_Integer n = 0;
		switch (field) {
		case ST_DEV: n = (lua_Integer)st->st_dev;
			break;
		case ST_RDEV: n = (lua_Integer)st->st_rdev;
			break;
		case ST_INO: n = (lua_Integer)st->st_ino;
			break;
		case ST_NLINK: n = (lua_Integer)st->st_nlink;
			break;
		case ST_UID:  n = (lua_Integer)st->st_uid;
			break;
		case ST_GID: n = (lua_Integer)st->st_gid;
			break;
		case ST_ATIME: n = (lua_Integer)st->st_atime;
			break;
		case ST_MTIME: n = (lua_Integer)st->st_mtime;
			break;
		case ST_CTIME: n = (lua_Integer)st->st_ctime;
			break;
		case ST_MODE: n = (lua_Integer)st->st_mode;
			break;
		case ST_SIZE: n = (lua_Integer)st->st_size;
			break;
		default:
			break;
		}
		lua_pushinteger(L, n);
		lua_pushinteger(L, 0);
	}
	return 2;
}

static int stat_retval(lua_State *L, struct stat *st, int err)
{
	if (err != 0) {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	} else {
		int fields = ST_ALL;
		if (lua_gettop(L) >= 2) {
			fields = lua_tointeger(L, 2);
		}
		lua_newtable(L);
		if (fields & ST_UID) {
			lua_pushstring(L,"uid");
			lua_pushinteger(L, st->st_uid);
			lua_settable(L, -3);
		}
		if (fields & ST_GID) {
			lua_pushstring(L,"gid");
			lua_pushinteger(L, st->st_gid);
			lua_settable(L, -3);
		}
		if (fields & ST_NLINK) {
			lua_pushstring(L,"nlink");
			lua_pushinteger(L, st->st_nlink);
			lua_settable(L, -3);
		}
		if (fields & ST_MODE) {
			lua_pushstring(L,"mode");
			lua_pushinteger(L, st->st_mode);
			lua_settable(L, -3);
		}
		if (fields & ST_DEV) {
			lua_pushstring(L,"dev");
			lua_pushinteger(L, st->st_dev);
			lua_settable(L, -3);
		}
		if (fields & ST_INO) {
			lua_pushstring(L,"ino");
			lua_pushinteger(L, st->st_ino);
			lua_settable(L, -3);
		}
		if (fields & ST_RDEV) {
			lua_pushstring(L,"rdev");
			lua_pushinteger(L, st->st_rdev);
			lua_settable(L, -3);
		}
		if (fields & ST_SIZE) {
			lua_pushstring(L,"size");
			lua_pushinteger(L, st->st_size);
			lua_settable(L, -3);
		}
		if (fields & ST_ATIME) {
			lua_pushstring(L,"atime");
			lua_pushinteger(L, st->st_atime);
			lua_settable(L, -3);
		}
		if (fields & ST_MTIME) {
			lua_pushstring(L,"mtime");
			lua_pushinteger(L, st->st_mtime);
			lua_settable(L, -3);
		}
		if (fields & ST_CTIME) {
			lua_pushstring(L,"ctime");
			lua_pushinteger(L, st->st_ctime);
			lua_settable(L, -3);
		}
		lua_pushinteger(L, 0);
	}
	return 2;
}

/*
** value, err = fs.ustat(path, field)
*/
static int lfs_ustat(lua_State *L)
{
	struct stat st;
	int err = stat(lua_tostring(L, 1), &st);
	return ustat_retval(L, &st, err);
}

/*
** { size = ..., atime = ..., ...}/nil = fs.stat(path)
*/
static int lfs_stat(lua_State *L)
{
	struct stat st;
	int err = stat(lua_tostring(L, 1), &st);
	return stat_retval(L, &st, err);
}

/*
** value, err = fs.ulstat(path)
*/
static int lfs_ulstat(lua_State *L)
{
	struct stat st;
	int err = lstat(lua_tostring(L, 1), &st);
	return ustat_retval(L, &st, err);
}

/*
** { size = ..., atime = ..., ...}/nil, err = fs.lstat(path)
*/
static int lfs_lstat(lua_State *L)
{
	struct stat st;
	int err = lstat(lua_tostring(L, 1), &st);
	return stat_retval(L, &st, err);
}

/*
** value, err = fs.ufstat(path)
*/
static int lfs_ufstat(lua_State *L)
{
	struct stat st;
	int err = fstat(lua_tointeger(L, 1), &st);
	return ustat_retval(L, &st, err);
}

/*
** { size = ..., atime = ..., ...}/nil, err = fs.fstat(path)
*/
static int lfs_fstat(lua_State *L)
{
	struct stat st;
	int err = fstat(luaL_checkinteger(L, 1), &st);
	return stat_retval(L, &st, err);
}

static void statvfs_buildtable(lua_State *L, struct statvfs *st)
{
	lua_newtable(L);
	lua_pushstring(L, "bsize");
	lua_pushinteger(L, st->f_bsize);
	lua_rawset(L, -3);
	lua_pushstring(L, "frsize");
	lua_pushinteger(L, st->f_frsize);
	lua_rawset(L, -3);
	lua_pushstring(L, "blocks");
	lua_pushinteger(L, st->f_blocks);
	lua_rawset(L, -3);
	lua_pushstring(L, "bfree");
	lua_pushinteger(L, st->f_bfree);
	lua_rawset(L, -3);
	lua_pushstring(L, "bavail");
	lua_pushinteger(L, st->f_bavail);
	lua_rawset(L, -3);
	lua_pushstring(L, "files");
	lua_pushinteger(L, st->f_files);
	lua_rawset(L, -3);
	lua_pushstring(L, "ffree");
	lua_pushinteger(L, st->f_ffree);
	lua_rawset(L, -3);
	lua_pushstring(L, "favail");
	lua_pushinteger(L, st->f_favail);
	lua_rawset(L, -3);
	lua_pushstring(L, "fsid");
	lua_pushinteger(L, st->f_fsid);
	lua_rawset(L, -3);
	lua_pushstring(L, "flag");
	lua_pushinteger(L, st->f_flag);
	lua_rawset(L, -3);
	lua_pushstring(L, "namemax");
	lua_pushinteger(L, st->f_namemax);
	lua_rawset(L, -3);
}

/*
** vfs_st, err = fs.statvfs(path)
*/
static int lfs_statvfs(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	struct statvfs st;
	if (statvfs(path, &st) == 0) {
		statvfs_buildtable(L, &st);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	return 2;
}

/*
** vfs_st, err = fs.fstatvfs(fd)
*/
static int lfs_fstatvfs(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	struct statvfs st;
	if (fstatvfs(fd, &st) == 0) {
		statvfs_buildtable(L, &st);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	return 2;
}

/*
** err = fs.fchdir(fd)
*/
static int lfs_fchdir(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int err = fchdir(fd);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.link(filepath, linkpath)
*/
static int lfs_link(lua_State *L)
{
	const char *filepath = luaL_checkstring(L, 1);
	const char *linkpath = luaL_checkstring(L, 2);
	int err = link(filepath, linkpath);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.symlink(filepath, linkpath)
*/
static int lfs_symlink(lua_State *L)
{
	const char *filepath = luaL_checkstring(L, 1);
	const char *linkpath = luaL_checkstring(L, 2);
	int err = symlink(filepath, linkpath);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.rename(old, new)
*/
static int lfs_rename(lua_State *L)
{
	const char *old = luaL_checkstring(L, 1);
	const char *new = luaL_checkstring(L, 2);
	int err = rename(old, new);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.remove(path)
*/
static int lfs_remove(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int err = remove(path);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** result, err = fs.readlink(path, buffer=nil)
**
** result can be the number of bytes read if buffer is valid,
** or be a string containing the content.
*/
static int lfs_readlink(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	struct stat st;
	int err = lstat(path, &st);
	if (err < 0) {
		err = errno;
	} else {
		size_t size = st.st_size;
		char *mem;
		bool has_buffer = false;
		ssize_t nread = 0;

		if (lua_type(L, 2) == LUA_TUSERDATA) {
			Buffer *buffer = buffer_lcheck(L, 2);
			mem = (char*)buffer_grow(buffer, size);
			has_buffer = true;
		} else {
			mem = (char*)MALLOC(size);
		}

		if (mem != NULL) {
			nread = readlink(path, mem, size);
			if (nread < 0)
				err = errno;
		} else {
			err = has_buffer ? EOVERFLOW : ENOMEM;
		}

		if (err == 0) {
			if (has_buffer)
				lua_pushinteger(L, (int)nread);
			else
				lua_pushlstring(L, mem, size);
		}

		if (!has_buffer && mem != NULL)
			FREE(mem);
	}

	if (err != 0)
		lua_pushnil(L);
	lua_pushinteger(L, err);

	return 2;
}

/*
** old_mode = fs.umask(mode)
*/
static int lfs_umask(lua_State *L)
{
	mode_t new_mode = (mode_t)luaL_checkinteger(L, 1);
	lua_pushinteger(L, (int)umask(new_mode));
	return 1;
}

/*
** err = fs.chown(path, uid=nil, gid=nil)
*/
static int lfs_chown(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	uid_t uid = (uid_t)luaL_optinteger(L, 2, -1);
	gid_t gid = (gid_t)luaL_optinteger(L, 3, -1);
	int err = chown(path, uid, gid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.fchown(fd, uid=nil, gid=nil)
*/
static int lfs_fchown(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	uid_t uid = (uid_t)luaL_optinteger(L, 2, -1);
	gid_t gid = (gid_t)luaL_optinteger(L, 3, -1);
	int err = fchown(fd, uid, gid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.lchown(path, uid=nil, gid=nil)
*/
static int lfs_lchown(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	uid_t uid = (uid_t)luaL_optinteger(L, 2, -1);
	gid_t gid = (gid_t)luaL_optinteger(L, 3, -1);
	int err = lchown(path, uid, gid);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.chmod(path, mode)
*/
static int lfs_chmod(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	mode_t mode = (mode_t)luaL_checkinteger(L, 2);
	int err = chmod(path, mode);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.fchmod(fd, mode)
*/
static int lfs_fchmod(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	mode_t mode = (mode_t)luaL_checkinteger(L, 2);
	int err = fchmod(fd, mode);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.truncate(path, length)
*/
static int lfs_truncate(lua_State *L)
{
	const char *path = luaL_checkstring(L, 2);
	off_t length = (off_t)luaL_checkinteger(L, 1);
	int err = truncate(path, length);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.ftruncate(fd, length)
*/
static int lfs_ftruncate(lua_State *L)
{
	int fd = luaL_checkinteger(L, 2);
	off_t length = (off_t)luaL_checkinteger(L, 1);
	int err = ftruncate(fd, length);
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fs.utimes(path, atime=nil, mtime=nil)
*/
static int lfs_utimes(lua_State *L)
{
	struct timeval tv_now = {0, 0};
	struct timeval times[2];
	const char *path = luaL_checkstring(L, 1);
	lua_Number atime = luaL_optnumber(L, 2, 0);
	lua_Number mtime = luaL_optnumber(L, 3, 0);
	int err;

	if (atime == 0) {
		gettimeofday(&tv_now, NULL);
		times[0] = tv_now;
	} else {
		times[0].tv_sec = (time_t)atime;
		times[0].tv_usec = (suseconds_t)((atime - times[0].tv_sec) * 1000000);
	}

	if (mtime == 0) {
		if (tv_now.tv_sec == 0 && tv_now.tv_usec == 0)
			gettimeofday(&tv_now, NULL);
		times[1] = tv_now;
	} else {
		times[1].tv_sec = (time_t)mtime;
		times[1].tv_usec = (suseconds_t)((mtime - times[1].tv_sec) * 1000000);
	}

	err = utimes(path, times);
	if (err < 0)
		err = errno;

	lua_pushinteger(L, err);
	return 1;
}

static const luaL_Reg dir_methods[] = {
	{"next", lfs_dir_next},
	{"close", lfs_dir_close},
	{"__gc", lfs_dir_close},
	{NULL, NULL}
};

static const luaL_Reg glob_methods[] = {
	{"next", lfs_glob_next},
	{"__gc", lfs_glob_gc},
	{NULL, NULL}
};

static const EnumReg enums[] = {
	LENUM(ST_DEV),
	LENUM(ST_RDEV),
	LENUM(ST_INO),
	LENUM(ST_NLINK),
	LENUM(ST_ATIME),
	LENUM(ST_CTIME),
	LENUM(ST_MTIME),
	LENUM(ST_MODE),
	LENUM(ST_UID),
	LENUM(ST_GID),
	LENUM(ST_SIZE),
	LENUM(ST_RDONLY),
	LENUM(ST_NOSUID),
	LENUM(F_OK),
	LENUM(R_OK),
	LENUM(W_OK),
	LENUM(X_OK),
	LENUM_NULL
};

static const luaL_Reg funcs[] = {
	{"basename", lfs_basename},
	{"dirname", lfs_dirname},
	{"realpath", lfs_realpath},

	{"getcwd", lfs_getcwd},
	{"chdir", lfs_chdir},
	{"fchdir", lfs_fchdir},
	{"dir", lfs_dir},
	{"listdir", lfs_listdir},
	{"glob", lfs_glob},

	{"link", lfs_link},
	{"symlink", lfs_symlink},
	{"mkdir", lfs_mkdir},
	{"mkdir_p", lfs_mkdir_p},
	{"unlink", lfs_unlink},
	{"rmdir", lfs_rmdir},
	{"remove", lfs_remove},
	{"rename", lfs_rename},

	{"stat",lfs_stat},
	{"ustat", lfs_ustat},
	{"lstat",lfs_lstat},
	{"ulstat", lfs_ulstat},
	{"fstat",lfs_fstat},
	{"ufstat", lfs_ufstat},
	{"statvfs", lfs_statvfs},
	{"fstatvfs", lfs_fstatvfs},
	{"access", lfs_access},
	{"readlink", lfs_readlink},

	{"umask", lfs_umask},
	{"chmod", lfs_chmod},
	{"fchmod", lfs_fchmod},
	{"chown", lfs_chown},
	{"fchown", lfs_fchown},
	{"lchown", lfs_lchown},
	{"truncate", lfs_truncate},
	{"ftruncate", lfs_ftruncate},
	{"utimes", lfs_utimes},

	{NULL, NULL}
};

int l_openfs(lua_State *L)
{
	l_register_metatable2(L, FS_DIR_META, dir_methods);
	l_register_metatable2(L, FS_GLOB_META, glob_methods);
	l_register_lib(L, "fs", funcs, enums);
	return 0;
}

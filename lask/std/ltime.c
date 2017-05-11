
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

static void push_tmtable(struct tm *tm, lua_State *L)
{
	lua_createtable(L, 0, 9);
	lua_pushstring(L, "year");
	lua_pushinteger(L, tm->tm_year);
	lua_rawset(L, -3);
	lua_pushstring(L, "mon");
	lua_pushinteger(L, tm->tm_mon);
	lua_rawset(L, -3);
	lua_pushstring(L, "mday");
	lua_pushinteger(L, tm->tm_mday);
	lua_rawset(L, -3);
	lua_pushstring(L, "hour");
	lua_pushinteger(L, tm->tm_hour);
	lua_rawset(L, -3);
	lua_pushstring(L, "min");
	lua_pushinteger(L, tm->tm_min);
	lua_rawset(L, -3);
	lua_pushstring(L, "sec");
	lua_pushinteger(L, tm->tm_sec);
	lua_rawset(L, -3);
	lua_pushstring(L, "wday");
	lua_pushinteger(L, tm->tm_wday);
	lua_rawset(L, -3);
	lua_pushstring(L, "yday");
	lua_pushinteger(L, tm->tm_yday);
	lua_rawset(L, -3);
	lua_pushstring(L, "isdst");
	lua_pushinteger(L, tm->tm_isdst);
	lua_rawset(L, -3);
}

/*
** {year=, mon=, day=, mday=, ...} = time.localtime(sec=now)
*/
static int ltime_localtime(lua_State *L)
{
	time_t when;
	struct tm tm_when;

	if (lua_gettop(L) > 0) {
		when = (time_t)luaL_checknumber(L, 1);
	} else {
		when = time(NULL);
	}

	localtime_r(&when, &tm_when);
	push_tmtable(&tm_when, L);
	return 1;
}

/*
** {year=, mon=, day=, mday=, ...} = time.gmtime(sec=now)
*/
static int ltime_gmtime(lua_State *L)
{
	time_t when;
	struct tm tm_when;

	if (lua_gettop(L) > 0) {
		when = (time_t)luaL_checknumber(L, 1);
	} else {
		when = time(NULL);
	}

	gmtime_r(&when, &tm_when);
	push_tmtable(&tm_when, L);
	return 1;
}

/*
** sec_since_unix_epoch = time.time()
*/
static int ltime_time(lua_State *L)
{
	struct timeval tv;
	lua_Number value;
	gettimeofday(&tv, NULL);
	value = (lua_Number)(tv.tv_sec + tv.tv_usec / 1000000.0);
	lua_pushnumber(L, value);
	return 1;
}

/*
** sec_since_unix_epoch, nano_sec = time.time2()
*/
static int ltime_time2(lua_State *L)
{
	struct timeval tv;
	lua_Number value;
	gettimeofday(&tv, NULL);
	lua_pushinteger(L, (lua_Integer)tv.tv_sec);
	lua_pushinteger(L, (lua_Integer)tv.tv_usec * 1000);
	return 2;
}

/*
** t = time.ctime(clock)
*/
static int ltime_ctime(lua_State *L)
{
	time_t clock = (time_t)luaL_checknumber(L, 1);
	char tmp[32];  /* at least 26 bytes in size, from posix standard */
	lua_pushstring(L, ctime_r(&clock, tmp));
	return 1;
}

/*
** str = time.strftime(fmt, clock)
*/
static int ltime_strftime(lua_State *L)
{
	const char *fmt = luaL_checkstring(L, 1);
	time_t clock = (time_t)luaL_checknumber(L, 2);
	char tmp[128];
	struct tm tm_clock;

	localtime_r(&clock, &tm_clock);
	strftime(tmp, sizeof(tmp), fmt, &tm_clock);
	lua_pushstring(L, tmp);
	return 1;
}

/*
** t = time.strptime(str, fmt)
*/
static int ltime_strptime(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const char *fmt = luaL_checkstring(L, 2);
	struct tm tm_clock;
	time_t clock;

	strptime(str, fmt, &tm_clock);
	clock = mktime(&tm_clock);
	lua_pushinteger(L, clock);
	return 1;
}

/*
** val = time.uptime()
*/
static int ltime_uptime(lua_State *L)
{
	struct timespec ts = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	lua_pushnumber(L, (lua_Number)ts.tv_sec + ts.tv_nsec / 1000000000.0);
	return 1;
}

/*
** sec, nano_sec =  time.uptime2()
*/
static int ltime_uptime2(lua_State *L)
{
	struct timespec ts = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	lua_pushinteger(L, (lua_Integer)ts.tv_sec);
	lua_pushinteger(L, (lua_Integer)ts.tv_nsec);
	return 2;
}

/*
** time.sleep(sec)
*/
static int ltime_sleep(lua_State *L)
{
	lua_Number sec = luaL_checknumber(L, 1);
	usleep((useconds_t)(sec * 1000000));
	return 0;
}

/*
** timespec = time.clock_gettime(clock_id)
*/
static int ltime_clock_gettime(lua_State *L)
{
	struct timespec ts;
	int clock_id = (int)luaL_checkinteger(L, 1);
	if (clock_gettime(clock_id, &ts) == 0) {
		lua_pushnumber(L, (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1000000000);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/*
** errno = time.setitimer(which, value, interval)
*/
static int ltime_setitimer(lua_State *L)
{
	int which = luaL_checkinteger(L, 1);
	lua_Number value = luaL_checknumber(L, 2);
	lua_Number interval = luaL_optnumber(L, 3, 0);
	struct itimerval itval;
	
	itval.it_value.tv_sec = (time_t)value;
	itval.it_value.tv_usec = (suseconds_t)((value - itval.it_value.tv_sec) * 1000000);
	itval.it_interval.tv_sec = (time_t)interval;
	itval.it_interval.tv_usec = (suseconds_t)((interval - itval.it_interval.tv_sec) * 1000000);
		
	lua_pushinteger(L, setitimer(which, &itval, NULL) ? errno : 0);
	return 1;
}

/*
** interval, value, errno = time.getitimer(which)
*/
static int ltime_getitimer(lua_State *L)
{
	int which = luaL_checkinteger(L, 1);
	struct itimerval itval;
	int err = getitimer(which, &itval);
	if (err == 0) {
		lua_pushnumber(L, itval.it_value.tv_sec + itval.it_value.tv_usec / 1000000.0);
		lua_pushnumber(L, itval.it_interval.tv_sec + itval.it_interval.tv_usec / 1000000.0);
	} else {
		err = errno;
		lua_pushnil(L);
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 3;
}

static const luaL_Reg funcs[] = {
	{"localtime", ltime_localtime},
	{"gmtime", ltime_gmtime},
	{"time", ltime_time},
	{"time2", ltime_time2},
	{"ctime", ltime_ctime},
	{"strptime", ltime_strptime},
	{"strftime", ltime_strftime},
	{"uptime", ltime_uptime},
	{"uptime2", ltime_uptime2},
	{"sleep", ltime_sleep},
	{"clock_gettime", ltime_clock_gettime},
	{"setitimer", ltime_setitimer},
	{"getitimer", ltime_getitimer},
	{NULL, NULL}
};

static const EnumReg enums[] = {
	LENUM(CLOCK_REALTIME),
	LENUM(ITIMER_REAL),
	LENUM(ITIMER_VIRTUAL),
	LENUM(ITIMER_PROF),
	LENUM_NULL
};

int l_opentime(lua_State *L)
{
	l_register_lib(L, "time", funcs, enums);	
	return 0;
}

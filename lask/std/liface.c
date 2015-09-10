
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>

/*
** ip_str, err = iface.getip(iface_name)
*/
static int liface_getip(lua_State *L)
{
	const char *ifname = luaL_checkstring(L, 1);
	int sock;
	char ip_str[16] = {0};

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock >= 0) {
		struct ifreq ifr;
		strcpy(ifr.ifr_name, ifname);
		
		if (ioctl(sock, SIOCGIFADDR, &ifr) >= 0) {
			struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
			inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
		}
		close(sock);
	}
	
	if (ip_str[0] != 0) {
		lua_pushstring(L, ip_str);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	
	return 2;
}

/*
** mac_str, err = iface.getip(iface_name)
*/
static int liface_getmac(lua_State *L)
{
	const char *ifname = luaL_checkstring(L, 1);
	int sock;
	char mac_str[20] = {0};

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock >= 0) {
		struct ifreq ifr;
		strcpy(ifr.ifr_name, ifname);
		
		if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0) {
			const uint8 *mac = (const uint8*)ifr.ifr_hwaddr.sa_data;
			snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		close(sock);
	}
	
	if (mac_str[0] != 0) {
		lua_pushstring(L, mac_str);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	
	return 2;
}

/*
** ifname = iface.getext()
*/
static int liface_getext(lua_State *L)
{
	FILE *input = fopen("/proc/net/route", "r");
	char device[16] = {0};
	char gw[16] = {0};

	while (!feof(input)) {
		/* XXX scanf(3) is unsafe, risks overrun */
		if ((fscanf(input, "%s %s %*s %*s %*s %*s %*s %*s %*s %*s %*s\n", device, gw) == 2) 
			&& strcmp(gw, "00000000") == 0) {
			break;
		}
	}
	fclose(input);
	
	if (strcmp(gw, "00000000") == 0)
		lua_pushstring(L, device);
	else
		lua_pushnil(L);
		
	return 1;
}

static const luaL_Reg funcs[] = {
	{"getext", liface_getext},
	{"getip", liface_getip},
	{"getmac", liface_getmac},
	{NULL, NULL}
};

int l_openiface(lua_State *L)
{
	l_register_lib(L, "iface", funcs, NULL);
	return 0;
}

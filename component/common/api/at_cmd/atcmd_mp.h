#ifndef __ATCMD_MP_H__
#define __ATCMD_MP_H__

#include <platform_opts.h>

#if CONFIG_BT
#define CONFIG_ATCMD_MP_EXT0	1 //support MP ext0 AT command
#else
#define CONFIG_ATCMD_MP_EXT0	0 //support MP ext0 AT command
#endif
#define CONFIG_ATCMD_MP_EXT1	0 //support MP ext1 AT command

typedef struct _at_command_mp_ext_item_{
	char	*mp_ext_cmd;
	int		(*mp_ext_fun)(void **argv, int argc);
	char	*mp_ext_usage;
}at_mp_ext_item_t;

#endif

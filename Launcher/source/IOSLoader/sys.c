#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gctypes.h>
#include <stdio.h>
#include <malloc.h>
#include <gccore.h>
#include <ogcsys.h>
#include "sys.h"

static fstats stats ATTRIBUTE_ALIGN(32);

bool AHBRPOT_Patched()
{
	return (*HW_AHBPROT == 0xFFFFFFFF);
}	

u8 *ISFS_GetFile(const char *path, u32 *size, s32 length)
{
	*size = 0;
	s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
	u8 *buf = NULL;

	if (fd >= 0)
	{
		memset(&stats, 0, sizeof(fstats));
		if (ISFS_GetFileStats(fd, &stats) >= 0)
		{
			if (length <= 0)
				length = stats.file_length;
			if (length > 0)
				buf = (u8 *)memalign(32, length);
			if (buf)
			{
				*size = stats.file_length;
				if (ISFS_Read(fd, (char*)buf, length) != length)
				{
					*size = 0;
					free(buf);
				}
			}
		}
		ISFS_Close(fd);
	}
	if (*size > 0)
	{
		DCFlushRange(buf, *size);
	}
	return buf;
}

bool _isNetworkAvailable()
{
	bool retval = false;
	u32 size;
	char ISFS_Filepath[32] ATTRIBUTE_ALIGN(32);
	strcpy(ISFS_Filepath, "/shared2/sys/net/02/config.dat");
	u8 *buf = ISFS_GetFile(ISFS_Filepath, &size, -1);
	if (buf && size > 4)
	{
		retval = buf[4] > 0;
		free(buf);
	}
	return retval;
}

bool ModeChecked = false;
bool DolphinMode = false;
bool IsDolphin(void)
{
	return false;
	if (ModeChecked)
		return DolphinMode;

	u32 ifpr11 = 0x12345678;
	u32 ifpr12 = 0x9abcdef0;
	u32 ofpr1 = 0x00000000;
	u32 ofpr2 = 0x00000000;
	asm volatile (
		"lwz 3,%[ifpr11]\n\t"
		"stw 3,8(1)\n\t"
		"lwz 3,%[ifpr12]\n\t"
		"stw 3,12(1)\n\t"

		"lfd 1,8(1)\n\t"
		"frsqrte	1, 1\n\t"
		"stfd 	1,8(1)\n\t"

		"lwz 	3,8(1)\n\t"
		"stw	3, %[ofpr1]\n\t"
		"lwz 	3,12(1)\n\t"
		"stw	3, %[ofpr2]\n\t"

		:
	[ofpr1]"=m" (ofpr1)
		, [ofpr2]"=m" (ofpr2)
		:
		[ifpr11]"m" (ifpr11)
		, [ifpr12]"m" (ifpr12)

		);
	if (ofpr1 != 0x56cc62b2)
	{
		DolphinMode = true;
	}
	else
	{
		DolphinMode = false;
	}
	ModeChecked = true;
	return DolphinMode;
}

//s32 __IOS_LoadStartupIOS() { return 0; }
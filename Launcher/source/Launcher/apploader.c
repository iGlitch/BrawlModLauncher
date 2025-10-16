#include <stdio.h>
#include <ogcsys.h>
#include <string.h>
#include <malloc.h>
#include "apploader.h"
#include "wdvd.h"
#include "patchcode.h"
#include "memory.h"
#include "video_tinyload.h"
#include "videopatch.h"

typedef int(*app_main)(void **dst, int *size, int *offset);
typedef void(*app_init)(void(*report)(const char *fmt, ...));
typedef void *(*app_final)();
//typedef void(*app_entry)(void(**init)(void(*report)(const char *fmt, ...)), int(**main)(), void *(**final)());
typedef void (*app_entry)(app_init *init, app_main *main, app_final *final);



static u8 *appldr = (u8 *)0x81200000;

#define APPLDR_OFFSET	0x910
#define APPLDR_CODE		0x918

static bool Remove_001_Protection(void *Address, int Size);

void maindolpatches(void *dst, int len, u8 vidMode, GXRModeObj *vmode, bool vipatch,
	bool countryString, u8 patchVidModes, int aspectRatio);

static struct
{
	char revision[16];
	void *entry;
	s32 size;
	s32 trailersize;
	s32 padding;
} apploader_hdr ATTRIBUTE_ALIGN(32);
u32 Apploader_Run(u8 vidMode, GXRModeObj *vmode, bool vipatch, bool countryString, u8 patchVidModes, int aspectRatio)
{
	void *dst = NULL;
	int len = 0;
	int offset = 0;
	u32 appldr_len;
	s32 ret;

	app_entry appldr_entry;
	app_init  appldr_init;
	app_main  appldr_main;
	app_final appldr_final;

	ret = WDVD_Read(&apploader_hdr, 0x20, APPLDR_OFFSET);
	//printf("apploader_Apploader_Run: WDVD_Read() return value = %d\n", ret);
	if (ret < 0)
		return 0;

	appldr_len = apploader_hdr.size + apploader_hdr.trailersize;
	//printf("apploader_Apploader_Run: appldr_len value = %08X\n", appldr_len);

	ret = WDVD_Read(appldr, appldr_len, APPLDR_CODE);
	//printf("apploader_Apploader_Run: WDVD_Read() return value = %d\n", ret);
	if (ret < 0)
		return 0;
	DCFlushRange(appldr, appldr_len);
	ICInvalidateRange(appldr, appldr_len);

	appldr_entry = apploader_hdr.entry;
	appldr_entry(&appldr_init, &appldr_main, &appldr_final);

	while (appldr_main(&dst, &len, &offset))
	{
		WDVD_Read(dst, len, offset);
		//printf("apploader_Apploader_Run: dst = %08X len = %08X offset = %08X\n",(u32)dst, (u32)len, (u32)offset);
		maindolpatches(dst, len, vidMode, vmode, vipatch, countryString, patchVidModes, aspectRatio);
		Remove_001_Protection(dst, len);
		DCFlushRange(dst, len);
		ICInvalidateRange(dst, len);
		prog(10);
	}

	return (u32)appldr_final();
}

void maindolpatches(void *dst, int len, u8 vidMode, GXRModeObj *vmode, bool vipatch, bool countryString, u8 patchVidModes, int aspectRatio)
{
	patchVideoModes(dst, len, vidMode, vmode, patchVidModes);
	dogamehooks(dst, len, false);
}

static bool Remove_001_Protection(void *Address, int Size)
{
	static const u8 SearchPattern[] = {0x40, 0x82, 0x00, 0x0C, 0x38, 0x60, 0x00, 0x01, 0x48, 0x00, 0x02, 0x44, 0x38, 0x61, 0x00, 0x18};
	static const u8 PatchData[] = {0x40, 0x82, 0x00, 0x04, 0x38, 0x60, 0x00, 0x01, 0x48, 0x00, 0x02, 0x44, 0x38, 0x61, 0x00, 0x18};
	u8 *Addr_end = Address + Size;
	u8 *Addr;

	for(Addr = Address; Addr <= Addr_end - sizeof SearchPattern; Addr += 4)
	{
		if(memcmp(Addr, SearchPattern, sizeof SearchPattern) == 0) 
		{
			memcpy(Addr, PatchData, sizeof PatchData);
			return true;
		}
	}
	return false;
}
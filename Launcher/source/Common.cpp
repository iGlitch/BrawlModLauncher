#include "Common.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "FileHolder.h"
#include <malloc.h>

extern u8 * configFileData;
extern int configFileSize;
extern u8 * infoFileData;
extern int infoFileSize;
extern char configBasePath[ISFS_MAXPATH];

const char* GetFileName(const char* path)
{
	int len = strlen(path);
	int origin = 0;
	for (int i = 0; i<len; i++)
	{
		if (path[i] == '-' || path[i] == '/')
			origin = i + 1;
		else if (path[i] == '.')
			return path + origin;
	}
	return NULL;
}

const char* GetExtension(const char* filepath)
{
	int len = strlen(filepath);
	for (int i = 0; i<len; i++)
	if (filepath[i] == '.')
		return filepath + i;
	return NULL;
}

void GetDirectory(const char* src, char* buf)
{
	int len = strlen(src);
	int mark = 0;
	for (int i = 0; i<len; i++)
	if (src[i] == '/')
		mark = i;
	else if (src[i] == '.')
		break;
	memcpy(buf, src, mark);
}

bool Contains(const char* target, const char* str)
{
	int len = strlen(target);
	int matchLen = strlen(str);
	for (int i = 0; i<len - matchLen + 1; i++)
	for (int j = 0; j<matchLen; j++)
	{
		if (target[i + j] != str[j])break;
		if (j == matchLen - 1)return true;
	}
	return false;
}

void ToLower(const char* str, char* buf)
{
	int len = strlen(str);
	for (int i = 0; i < len; i++)
		buf[i] = tolower(str[i]);
}

void loadInfoFile()
{
    char infoPath[ISFS_MAXPATH];
    memset(infoPath, 0, sizeof(infoPath));

    size_t len = strlen(configBasePath);
    if (len && configBasePath[len - 1] == '/')
        snprintf(infoPath, sizeof(infoPath), "%sinfo.xml", configBasePath);
    else
        snprintf(infoPath, sizeof(infoPath), "%s/info.xml", configBasePath);

    FileHolder infoFile(infoPath, "r");
    if (infoFile.IsOpen())
    {
        if (infoFileSize != 0)
            delete infoFileData;
        infoFileSize = infoFile.Size();
        infoFileData = (u8*)malloc(infoFileSize);
        memset(infoFileData, 0, infoFileSize);

        infoFile.FRead(infoFileData, infoFileSize, 1);
        infoFile.FClose();
    }
}

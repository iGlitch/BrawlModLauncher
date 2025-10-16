/****************************************************************************
 * Copyright (C) 2009-2011 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <ogcsys.h>
#include <string.h>
#include <stdio.h>
#include <debug.h>

/*#include "Prompts/PromptWindows.h"
#include "Prompts/ProgressWindow.h"
#include "FileOperations/fileops.h"
#include "TextOperations/wstring.hpp"*/
#include "wstring.hpp"
#include "CreateSubfolder.h"
#include "7ZipFile.h"

// 7zip error list
//static const char * szerrormsg[10] = {
//   tr("File is corrupt."), // 7z: Data error
//   tr("Not enough memory."), // 7z: Out of memory
//   tr("File is corrupt (CRC mismatch)."), // 7z: CRC Error
//   tr("File uses unsupported compression settings."), // 7z: Not implemented
//   tr("File is corrupt."), // 7z: Fail
//   tr("Failed to read file data."), // 7z: Data read failure
//   tr("File is corrupt."), // 7z: Archive error
//   tr("File uses too high of compression settings (dictionary size is too large)."), // 7z: Dictionary too large
//   tr("Can't open file."),
//   tr("Process canceled."),
//};

#define MAXPATHLEN 256

SzFile::SzFile(const char *filepath)
{
	memset(&CurArcFile, 0, sizeof(CurArcFile));
	SzResult = SZ_OK;
	if(InFile_Open(&archiveStream.file, filepath))
	{
		SzResult = 9;
		return;
	}

	FileInStream_CreateVTable(&archiveStream);
	LookToRead_CreateVTable(&lookStream, False);

	lookStream.realStream = &archiveStream.s;
	LookToRead_Init(&lookStream);

	// set default 7Zip SDK handlers for allocation and freeing memory
	SzAllocImp.Alloc = SzAlloc;
	SzAllocImp.Free = SzFree;
	SzAllocTempImp.Alloc = SzAllocTemp;
	SzAllocTempImp.Free = SzFreeTemp;

	// prepare CRC and 7Zip database structures
	CrcGenerateTable();
	SzArEx_Init(&SzArchiveDb);

	// open the archive
	SzResult = SzArEx_Open(&SzArchiveDb, &lookStream.s, &SzAllocImp, &SzAllocTempImp);
	if (SzResult != SZ_OK)
	{
		DisplayError(SzResult);
	}
}

SzFile::~SzFile()
{
	SzArEx_Free(&SzArchiveDb, &SzAllocImp);

	File_Close(&archiveStream.file);

	if(CurArcFile.filename != NULL)
		free(CurArcFile.filename);
}

char *SzFile::GetUtf8Filename(int ind)
{
	if(ind > (int) SzArchiveDb.db.NumFiles || ind < 0)
		return NULL;

	char *filename = NULL;

	int len = SzArEx_GetFileNameUtf16(&SzArchiveDb, ind, 0);
	if (len != 0)
	{
		UInt16 *utf16Name = (UInt16 *)SzAlloc(NULL, len * sizeof(UInt16));
		if(!utf16Name)
			return NULL;

		SzArEx_GetFileNameUtf16(&SzArchiveDb, ind, utf16Name);

		wString wName;
		wName.reserve(len);

		for(int i = 0; i < len; i++)
			wName.push_back(utf16Name[i]);

		SzFree(NULL, utf16Name);

		filename = strdup(wName.toUTF8().c_str());
	}

	return filename;
}

bool SzFile::Is7ZipFile (const char *buffer)
{
	// 7z signature
	int i;
	for(i = 0; i < 6; i++)
		if(buffer[i] != k7zSignature[i])
			return false;

	return true; // 7z archive found
}

ArchiveFileStruct * SzFile::GetFileStruct(int ind)
{
	if(ind > (int) SzArchiveDb.db.NumFiles || ind < 0)
		return NULL;

	char *filename = GetUtf8Filename(ind);

	if(CurArcFile.filename != NULL)
		free(CurArcFile.filename);

	const CSzFileItem * SzFileItem = SzArchiveDb.db.Files + ind;
	CurArcFile.filename = filename ? filename : strdup("");
	CurArcFile.length = SzFileItem->Size;
	CurArcFile.comp_length = 0;
	CurArcFile.isdir = SzFileItem->IsDir;
	CurArcFile.fileindex = ind;
	if(SzFileItem->MTimeDefined)
		CurArcFile.ModTime = (u64) (SzFileItem->MTime.Low  | ((u64) SzFileItem->MTime.High << 32));
	else
		CurArcFile.ModTime = 0;
	CurArcFile.archiveType = SZIP;

	return &CurArcFile;
}

void SzFile::DisplayError(SRes res)
{
	//const char *cpErrString = (res > 0 && res) < 10 ? szerrormsg[(res - 1)] : "";
	//ThrowMsg(tr("7z decompression failed:"), "%s %i: %s", tr("Error"), res, cpErrString);
}

u32 SzFile::GetItemCount()
{
	if(SzResult != SZ_OK)
		return 0;

	return SzArchiveDb.db.NumFiles;
}
/*
void* cur=NULL;
int pos=0;
void* AllocBuf(void*,size_t size)
{
	pos+=size;
	return (void*)((uint)cur+pos-size);
}

void FreeBuf(void* ,void* p)
{
}*/

int SzFile::ExtractIntoMemory(int fileindex,u8* dst,int dstSize)
{
	if(SzResult != SZ_OK)
		return -1;

	if(!GetFileStruct(fileindex))
		return -2;

	UInt32 SzBlockIndex = 0xFFFFFFFF;
	size_t SzOffset = 0;
	size_t SzSizeProcessed = 0;
	Byte * outBuffer = 0;
	size_t outBufferSize = 0;

	if(CurArcFile.isdir)return 0;

	/*auto prevA=SzAllocImp.Alloc;
	auto prevF=SzAllocImp.Free;
	SzAllocImp.Alloc=AllocBuf;
	SzAllocImp.Free=FreeBuf;
	cur=dst;*/

	// Extract
	SzResult = SzArEx_Extract(&SzArchiveDb, &lookStream.s, fileindex,
								&SzBlockIndex, &outBuffer, &outBufferSize,
								&SzOffset, &SzSizeProcessed,
								&SzAllocImp, &SzAllocTempImp);
	/*cur=NULL;
	SzAllocImp.Alloc=prevA;
	SzAllocImp.Free=prevF;*/
	memcpy(dst,outBuffer,dstSize>(int)CurArcFile.length?CurArcFile.length:dstSize);
	int size=
		SzResult==SZ_OK
		?(dstSize>(int)CurArcFile.length?CurArcFile.length:dstSize)
		:-1;
	if(outBuffer)
	{
		IAlloc_Free(&SzAllocImp, outBuffer);
		outBuffer = NULL;
	}

	// check for errors
	if(SzResult != SZ_OK)
	{
		// display error message
		DisplayError(SzResult);
		return -4;
	}
	return size;
}

int SzFile::ExtractFile(int fileindex, const char * outpath, bool withpath)
{
	if(SzResult != SZ_OK)
		return -1;

	if(!GetFileStruct(fileindex))
		return -2;

	// reset variables
	UInt32 SzBlockIndex = 0xFFFFFFFF;
	size_t SzOffset = 0;
	size_t SzSizeProcessed = 0;
	Byte * outBuffer = 0;
	size_t outBufferSize = 0;

	char * RealFilename = strrchr(CurArcFile.filename, '/');

	char writepath[MAXPATHLEN];
	if(!RealFilename || withpath)
		snprintf(writepath, sizeof(writepath), "%s/%s", outpath, CurArcFile.filename);
	else
		snprintf(writepath, sizeof(writepath), "%s/%s", outpath, RealFilename+1);

	if(CurArcFile.isdir)
	{
		strncat(writepath, "/", sizeof(writepath));
		CreateSubfolder(writepath);
		return 1;
	}

	char * temppath = strdup(writepath);
	char * pointer = strrchr(temppath, '/');
	if(pointer)
	{
		pointer += 1;
		pointer[0] = '\0';
	}
	CreateSubfolder(temppath);

	free(temppath);
	temppath = NULL;

	//startup timer
	//ShowProgress(0, CurArcFile.length, (RealFilename ? RealFilename+1 : CurArcFile.filename));

	// Extract
	SzResult = SzArEx_Extract(&SzArchiveDb, &lookStream.s, fileindex,
								&SzBlockIndex, &outBuffer, &outBufferSize,
								&SzOffset, &SzSizeProcessed,
								&SzAllocImp, &SzAllocTempImp);

	/*if(ProgressWindow::Instance()->IsCanceled())
		SzResult = 10;*/

	if(SzResult == SZ_OK)
	{
		FILE * wFile = fopen(writepath, "wb");

		//not quite right and needs to be changed
		u32 done = 0;
		if(!wFile)
			done = CurArcFile.length;

		do
		{
			//ShowProgress(done, CurArcFile.length, (RealFilename ? RealFilename+1 : CurArcFile.filename));
			int remain=CurArcFile.length-done;
			int wrote = fwrite(outBuffer+done, 1,remain>51200?51200:remain, wFile);
			done += wrote;

			if(wrote == 0)
				break;

		} while(done < CurArcFile.length);

		fclose(wFile);
	}
	if(outBuffer)
	{
		IAlloc_Free(&SzAllocImp, outBuffer);
		outBuffer = NULL;
	}

	// finish up the progress for this file
	//FinishProgress(CurArcFile.length);

	// check for errors
	if(SzResult != SZ_OK)
	{
		// display error message
		DisplayError(SzResult);
		return -4;
	}

	return 1;
}

int SzFile::ExtractAll(const char * destpath)
{
	if(!destpath)
		return -5;

	for(u32 i = 0; i < SzArchiveDb.db.NumFiles; i++)
	{
		/*if(ProgressWindow::Instance()->IsCanceled())
			return PROGRESS_CANCELED;*/

		CSzFileItem * SzFileItem = SzArchiveDb.db.Files + i;

		char *tmpName = GetUtf8Filename(i);

		char path[MAXPATHLEN];

		if(tmpName) {
			snprintf(path, sizeof(path), "%s/%s", destpath, tmpName);
			free(tmpName);
		}
		else
			snprintf(path, sizeof(path), "%s", destpath);

		u32 filesize = SzFileItem->Size;

		if(!filesize)
			continue;

		char * pointer = strrchr(path, '/')+1;

		if(pointer)
		{
			//cut off filename
			pointer[0] = '\0';
		}
		else
			continue; //shouldn't ever happen but to be sure, skip the file if it does

		//CreateSubfolder(path);

		ExtractFile(i, path);
	}

	return 1;
}

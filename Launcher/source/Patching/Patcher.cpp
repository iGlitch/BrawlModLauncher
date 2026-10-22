#include <stdio.h>
#include <stdlib.h>
#include <gctypes.h>
#include <dirent.h>
#include <debug.h>
#include <vector>
#include <memory>
#include <assert.h>
#include <malloc.h>
#include <di/di.h>
#include "ApplyPatch.h"
#include "Patcher.h"
//#include "DVDISO.h"  // Disc loading moved to loader.dol
#include "7z/7ZipFile.h"
#include "FrozenMemories.h"
#include "7z/CreateSubfolder.h"
#include "tinyxml2.h"
#include "../FileHolder.h"
#include "../Common.h"
#include "md5.h"
//#include "../Launcher/wdvd.h"  // Disc loading moved to loader.dol
//#include "../Launcher/disc.h"   // Disc loading moved to loader.dol

// Disc FST includes commented out - disc loading moved to loader.dol
//extern "C"{
//#include "fst_J.h"
//#include "fst_U.h"
//}
using namespace tinyxml2;


void ToPath(const char* src, char* out)
{
	for (int i = 0; src[i] != '\0'; i++)
	{
		if (src[i] == '-')
			out[i] = '/';
		else
			out[i] = src[i];
	}
}

int SearchFile(const std::vector<const char*>& arcs, const char* file)
{
	int len = arcs.size();
	for (int i = 0; i < len; i++)
	{
		const char* filename = arcs[i];
		if (filename != NULL && strcasecmp(filename, file) == 0)
			return i;
	}
	return -1;
}

void WriteHash(const byte* src, int len, char result[32])
{
	byte hash[16];
	md5_state_t state;
	md5_init(&state);
	md5_append(&state, src, len);
	md5_finish(&state, hash);
	
	memset(result, 0, 32);
	for (int i = 0; i < 16; i++)
		sprintf(result + i * 2, "%02x", hash[i]);
}

void WriteHash(const char* src, char result[32])
{
	FileHolder f(src, "rb");
	int len = f.Size();
	std::shared_ptr<byte> buf(new byte[len]);
	f.FRead(buf.get(), len, 1);
	WriteHash(buf.get(), len, result);
	f.FClose();
}


/* Disc system code commented out - disc loading moved to loader.dol
static struct {
	u32 offset;
	u32 type;
} partition_table[32] ATTRIBUTE_ALIGN(32);

static struct {
	u32 count;
	u32 offset;
	u32 pad[6];
} part_table_info ATTRIBUTE_ALIGN(32);
*/
bool isDiscSystemPrepared = false;
/* Disc system functions commented out - disc loading moved to loader.dol
bool prepareDiscSystem(wchar_t * sCurrentInfoText)
{
	//get partition info


	swprintf(sCurrentInfoText, 255, L"Preparing disc drive...");
	//Disc_SetLowMemPre();
	WDVD_Init();

	swprintf(sCurrentInfoText, 255, L"Loading disc...");
	sleep(2);

	if (Disc_Open() != 0) {
		swprintf(sCurrentInfoText, 255, L"Cannot open disc");
		sleep(2);
		return false;
	}

	u32 offset = 0;
	u32 GameIOS = 0;
	swprintf(sCurrentInfoText, 255, L"Searching for game partition...");
	Disc_FindPartition(&offset);
	swprintf(sCurrentInfoText, 255, L"Opening disc partition...");
	WDVD_OpenPartition(offset, &GameIOS);
	isDiscSystemPrepared = true;
	return true;
}

void releaseDiscSystem()
{
	//WDVD_ClosePartition();
	WDVD_Close();
	WDVD_Reset();
	isDiscSystemPrepared = false;
}
*/

/* Disc FST search functions commented out - disc loading moved to loader.dol
FileEntry searchDiscFST(const char* file)
{
	static FileEntry dummy;
	dummy.Len = 0;

	const u8* fst = fst_U;

	FileEntry* root = (FileEntry*)fst;
	FileEntry * cur = root;
	u32 offNames = root->Len*sizeof(FileEntry);

	char buf[256];
	char currentName[256];
	strcpy(buf, file);
	cur++;
	char * toSearch = strtok(buf, "/");
	char * nextPart = strtok(NULL, "/");

	while ((u32)cur < (u32)(fst + offNames))
	{
		if (cur->Type == 0) //file
		{
			cur++;
			continue;
		}
		int offset = (cur->NameOffset[0] << 16) + (cur->NameOffset[1] << 8) + cur->NameOffset[2];
		strcpy(currentName, (char*)&fst[offNames + offset]);
		if (strcasecmp(currentName, toSearch) == 0)
		{
			cur++;

			toSearch = nextPart;
			nextPart = strtok(NULL, "/");
			if (nextPart == NULL)
				break;
		}
		else if (cur != root)
		{
			cur = (FileEntry*)((u32)root + cur->Len*sizeof(FileEntry));
		}
	}


	while ((u32)cur < (u32)(fst + offNames))
	{
		if (cur->Type == 1)
			return dummy;

		int offset = (cur->NameOffset[0] << 16) + (cur->NameOffset[1] << 8) + cur->NameOffset[2];
		strcpy(currentName, (char*)&fst[offNames + offset]);
		if (strcasecmp(currentName, toSearch) == 0)
			return *cur;
		cur++;
	}
	return dummy;
}

int getSizeFromFST(const char* file)
{
	FileEntry result = searchDiscFST(file);
	return result.Len == UINT_MAX ? -1 : (int)result.Len;
}
*/

bool PMPatchVerify(const char* sPatchFilePath, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{
	swprintf(sCurrentInfoText, 255, L"Loading patch file...");
	SzFile sZip(sPatchFilePath);	
	//get file information
	u32 numItems = sZip.GetItemCount();
	std::vector<const char*> names;
	names.reserve(numItems);
	u8* xmlFile;
	size_t xmlFileLength;
	for (int i = 0; i < (int)numItems; i++)
	{
		ArchiveFileStruct* st = sZip.GetFileStruct(i);
		names.push_back(st->filename);
		if (strcasecmp(st->filename, "update.xml") == 0)
		{
			xmlFileLength = st->length;
			xmlFile = (u8*)memalign(32, xmlFileLength);
			sZip.ExtractIntoMemory(i, xmlFile, xmlFileLength);
		}
		swprintf(sCurrentInfoText, 255, L"Logging %s", st->filename);
		//sleep(5);
	}

	//extract patch table
	if (xmlFile == NULL)
	{
		swprintf(sCurrentInfoText, 255, L"Failed to export replacement table.");
		sleep(5);
		return false;
	}
	swprintf(sCurrentInfoText, 255, L"Opening patch table...");

	//Open patch table

	XMLDocument doc;
	if (doc.Parse((const char *)xmlFile, xmlFileLength) != (int)tinyxml2::XML_NO_ERROR)
	{
		swprintf(sCurrentInfoText, 255, L"Could not open /update.xml as an XML");
		sleep(5);
		return false;
	}

	XMLElement* cur = doc.RootElement();
	if (!cur)
	{
		swprintf(sCurrentInfoText, 255, L"No root element found in /update.xml");
		sleep(5);
		return false;
	}
	cur = cur->FirstChildElement("file");



	swprintf(sCurrentInfoText, 255, L"Loading files list...");

	uint uiItemCount = 0;
	while (cur)
	{
		const char * destinationFile = cur->Attribute("destinationFile");

		uiItemCount++;
		cur = cur->NextSiblingElement("file");
	}
	cur = doc.RootElement();
	cur = cur->FirstChildElement("file");

	swprintf(sCurrentInfoText, 255, L"Found %d files...", uiItemCount);


	int currentResource = 0;
	char md5Hash[32];
	bool allFilesVerified = true;

	while (cur && allFilesVerified)//patch main loop
	{
		if (bForceCancel == true)
			break;

		fProgressPercentage = (f32)currentResource / uiItemCount;

		while (true)
		{

			const char* patchMethod = NULL;
			patchMethod = cur->Attribute("method");
			if (strcasecmp(patchMethod, "patch") != 0)
				break;

			swprintf(sCurrentInfoText, 255, L"Loading patch information...");

			u8 patchType = atoi(cur->Attribute("patchType"));
			const char * sourceLocation = cur->Attribute("sourceLocation");
			const char * sourceFile = cur->Attribute("sourceFile");
			u32 sourceLength = atoi(cur->Attribute("sourceLength"));
			const char * sourceMD5 = cur->Attribute("sourceMD5");
			const char * updateFile = cur->Attribute("updateFile");
			const char * destinationFile = cur->Attribute("destinationFile");
			const char * destinationMD5 = cur->Attribute("destinationMD5");
			//continue;
			bool isDVD = sourceLocation != NULL && strcasecmp(sourceLocation, "disc") == 0;

			swprintf(sCurrentInfoText, 255, L"Checking for file: %s", destinationFile);

			FILE * existCheckFile = fopen(destinationFile, "rb");
			if (existCheckFile != NULL)
			{
				swprintf(sCurrentInfoText, 255, L"Verifying file: %s", destinationFile);
				fseek(existCheckFile, 0, SEEK_END);
				int existingFileSize = ftell(existCheckFile);
				u8* existingFileBuffer = (u8*)memalign(32, existingFileSize);
				fseek(existCheckFile, 0, SEEK_SET);
				fread(existingFileBuffer, existingFileSize, 1, existCheckFile);
				WriteHash(existingFileBuffer, existingFileSize, md5Hash);
				free(existingFileBuffer);
				fclose(existCheckFile);
				if (strcasecmp(destinationMD5, md5Hash) == 0)
				{
					swprintf(sCurrentInfoText, 255, L"Skipping file: %s", destinationFile);
					break;
				}
			}


			//searchFile isn't working...

			int fileIndex = -1;
			for (int i = 0, len = sZip.GetItemCount(); i < len; i++)
			{
				const char* filename = sZip.GetFileStruct(i)->filename;
				if (filename != NULL && strcasecmp(filename, updateFile) == 0)
				{
					fileIndex = i;
					break;
				}
			}

			if (isDVD)
				break;


			FILE * fSource = fopen(sourceFile, "rb");
			if (!fSource)
			{
				swprintf(sCurrentInfoText, 255, L"File not found: %s", sourceFile);
				allFilesVerified = false;
				break;
			}
			fseek(fSource, 0, SEEK_END);
			u32 sdSourceSize;
			sdSourceSize = ftell(fSource);
			if (sdSourceSize != sourceLength)
			{
				fclose(fSource);
				swprintf(sCurrentInfoText, 255, L"File Length Mismatch: %s", sourceFile);
				allFilesVerified = false;
				break;
			}
			u8 * sourceFileBuffer = (u8*)memalign(32, sdSourceSize);
			fseek(fSource, 0, SEEK_SET);
			fread(sourceFileBuffer, sizeof(u8), sdSourceSize, fSource);
			fclose(fSource);

			WriteHash(sourceFileBuffer, sdSourceSize, md5Hash);
			free(sourceFileBuffer);
			if (strcasecmp(sourceMD5, md5Hash) != 0)
			{
				swprintf(sCurrentInfoText, 255, L"Source MD5 Mismatch: %s", sourceFile);
				allFilesVerified = false;
				break;
			}
			break;

		}
		if (!allFilesVerified)
			break;
		swprintf(sCurrentInfoText, 255, L"Loading next file...");
		cur = cur->NextSiblingElement("file");
		currentResource++;
	}


	//resumeW.FClose();
	free(xmlFile);

	if (bForceCancel == true)
		return false;
	f32 value = 1.0f;


	//remove("/resume.bin");
	if (allFilesVerified)
		swprintf(sCurrentInfoText, 255, L"Verification complete!");


	return allFilesVerified;

}

bool PMPatch(const char* sPatchFilePath, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{

	swprintf(sCurrentInfoText, 255, L"Loading patch file...");
	SzFile sZip(sPatchFilePath);
	//get file information
	u32 numItems = sZip.GetItemCount();
	std::vector<const char*> names;
	names.reserve(numItems);
	u8* xmlFile;
	size_t xmlFileLength;
	for (int i = 0; i < (int)numItems; i++)
	{
		ArchiveFileStruct* st = sZip.GetFileStruct(i);
		names.push_back(st->filename);
		if (strcasecmp(st->filename, "update.xml") == 0)
		{
			xmlFileLength = st->length;
			xmlFile = (u8*)memalign(32, xmlFileLength);
			sZip.ExtractIntoMemory(i, xmlFile, xmlFileLength);
		}
		swprintf(sCurrentInfoText, 255, L"Logging %s", st->filename);
		//sleep(5);
	}

	//extract patch table
	if (xmlFile == NULL)
	{
		swprintf(sCurrentInfoText, 255, L"Failed to export replacement table.");
		sleep(5);
		return false;
	}
	swprintf(sCurrentInfoText, 255, L"Opening patch table...");

	//Open patch table

	XMLDocument doc;
	if (doc.Parse((const char *)xmlFile, xmlFileLength) != (int)tinyxml2::XML_NO_ERROR)
	{
		swprintf(sCurrentInfoText, 255, L"Could not open /update.xml as an XML");
		sleep(5);
		return false;
	}

	XMLElement* cur = doc.RootElement();
	if (!cur)
	{
		swprintf(sCurrentInfoText, 255, L"No root element found in /update.xml");
		sleep(5);
		return false;
	}
	cur = cur->FirstChildElement("file");



	swprintf(sCurrentInfoText, 255, L"Loading files list...");

	uint uiItemCount = 0;
	while (cur)
	{
		const char * destinationFile = cur->Attribute("destinationFile");

		DECLSTR(dstFullDir);
		GetDirectory(destinationFile, dstFullDir);

		swprintf(sCurrentInfoText, 255, L"Creating folder structure: %s", dstFullDir);

		CreateSubfolder(dstFullDir);

		uiItemCount++;
		cur = cur->NextSiblingElement("file");
	}
	cur = doc.RootElement();
	cur = cur->FirstChildElement("file");

	swprintf(sCurrentInfoText, 255, L"Found %d files...", uiItemCount);


	int currentResource = 0;
	char md5Hash[32];
	std::vector<FILE *> openedFiles;
	openedFiles.reserve(uiItemCount);

	while (cur)//patch main loop
	{
		if (bForceCancel == true)
			break;

		fProgressPercentage = (f32)currentResource / uiItemCount;

		//if (currentResource >= resumeNumber){

		//Save current resource number
		/*resumeW.FSeek(0, SEEK_SET);
		resumeW.FWrite(&currentResource, sizeof(int), 1);
		resumeW.FSync();*/

		const char* patchMethod = NULL;
		patchMethod = cur->Attribute("method");
		if (strcasecmp(patchMethod, "patch") == 0)
		{
			swprintf(sCurrentInfoText, 255, L"Loading patch information...");
			//goto loadNextCursor;
			u8 patchType = atoi(cur->Attribute("patchType"));
			const char * sourceLocation = cur->Attribute("sourceLocation");
			const char * sourceFile = cur->Attribute("sourceFile");
			u32 sourceLength = atoi(cur->Attribute("sourceLength"));
			const char * sourceMD5 = cur->Attribute("sourceMD5");
			const char * updateFile = cur->Attribute("updateFile");
			const char * destinationFile = cur->Attribute("destinationFile");
			const char * destinationMD5 = cur->Attribute("destinationMD5");
			//continue;
			bool isDVD = sourceLocation != NULL && strcasecmp(sourceLocation, "disc") == 0;

			swprintf(sCurrentInfoText, 255, L"Checking for file: %s", destinationFile);

			FILE * existCheckFile = fopen(destinationFile, "rb");
			if (existCheckFile != NULL)
			{
				swprintf(sCurrentInfoText, 255, L"Verifying file: %s", destinationFile);
				fseek(existCheckFile, 0, SEEK_END);
				int existingFileSize = ftell(existCheckFile);
				u8* existingFileBuffer = (u8*)memalign(32, existingFileSize);
				fseek(existCheckFile, 0, SEEK_SET);
				fread(existingFileBuffer, existingFileSize, 1, existCheckFile);
				//openedFiles.push_back(existCheckFile);
				WriteHash(existingFileBuffer, existingFileSize, md5Hash);
				free(existingFileBuffer);
				fclose(existCheckFile);
				if (strcasecmp(destinationMD5, md5Hash) == 0)
				{
					swprintf(sCurrentInfoText, 255, L"Skipping file: %s", destinationFile);
					goto loadNextCursor;
				}
			}



			swprintf(sCurrentInfoText, 255, L"Replacing: %s", destinationFile);



			//searchFile isn't working...
			int fileIndex = -1;
			for (int i = 0, len = sZip.GetItemCount(); i < len; i++)
			{
				const char* filename = sZip.GetFileStruct(i)->filename;
				if (filename != NULL && strcasecmp(filename, updateFile) == 0)
				{
					fileIndex = i;
					break;
				}
			}

			u8 * sourceFileBuffer;
			/* Disc loading code commented out - disc loading moved to loader.dol
			if (isDVD)
			{
				if (!isDiscSystemPrepared)
				{

					if (!prepareDiscSystem(sCurrentInfoText))
					{
						swprintf(sCurrentInfoText, 255, L"Could not prepare disc system");
						sleep(2);
						goto loadNextCursor;
					}
				}

				swprintf(sCurrentInfoText, 255, L"Searching disc for %s...", sourceFile);
				FileEntry sourceFileEntry = searchDiscFST(sourceFile);

				if (sourceFileEntry.Len != sourceLength)
				{
					if (sourceFileEntry.Len == 0)
						swprintf(sCurrentInfoText, 255, L"%s was not found", sourceFile);
					else
						swprintf(sCurrentInfoText, 255, L"File Length Mismatch: %s", sourceFile);
					sleep(2);
					goto loadNextCursor;
				}
				sourceFileBuffer = (u8*)memalign(32, sourceFileEntry.Len);
				memset(sourceFileBuffer, 0, sourceFileEntry.Len);

				swprintf(sCurrentInfoText, 255, L"Loading %s...", sourceFile);
				WDVD_Read(sourceFileBuffer, sourceFileEntry.Len, sourceFileEntry.Offset); //memory leaks

				//DI_Read(sourceFileBuffer, sourceFileEntry.Len, sourceFileEntry.Offset);


			}
			else
			*/
			{

				FILE * fSource = fopen(sourceFile, "rb");
				if (!fSource)
				{
					swprintf(sCurrentInfoText, 255, L"%s was not found", sourceFile);
					sleep(2);
					goto loadNextCursor;
				}
				//openedFiles.push_back(fSource);
				fseek(fSource, 0, SEEK_END);
				u32 sdSourceSize = ftell(fSource);
				if (sdSourceSize != sourceLength)
				{
					fclose(fSource);
					swprintf(sCurrentInfoText, 255, L"File Length Mismatch: %s", sourceFile);
					sleep(2);
					goto loadNextCursor;
				}
				sourceFileBuffer = (u8*)malloc(sdSourceSize);
				memset(sourceFileBuffer, 0, sdSourceSize);
				fseek(fSource, 0, SEEK_SET);
				fread(sourceFileBuffer, sizeof(u8), sdSourceSize, fSource);
				fclose(fSource);
			}
			memset(md5Hash, 0, 32);
			WriteHash(sourceFileBuffer, sourceLength, md5Hash);
			if (strcasecmp(sourceMD5, md5Hash) != 0)
			{
				swprintf(sCurrentInfoText, 255, L"Source MD5 Mismatch: %s", sourceFile);
				sleep(2);
				free(sourceFileBuffer);
				goto loadNextCursor;
			}

			ArchiveFileStruct * curArchive = sZip.GetFileStruct(fileIndex);
			if (!curArchive)
			{

				swprintf(sCurrentInfoText, 255, L"Couldn't find %s", updateFile);
				sleep(2);
				free(sourceFileBuffer);
				goto loadNextCursor;
			}
			swprintf(sCurrentInfoText, 255, L"Reading patch file: %s", updateFile);
			int updateFileBufferLength = curArchive->length;
			u8* updateFileBuffer = (u8*)memalign(32, updateFileBufferLength);
			memset(updateFileBuffer, 0, updateFileBufferLength);

			sZip.ExtractIntoMemory(curArchive->fileindex, updateFileBuffer, updateFileBufferLength);
			swprintf(sCurrentInfoText, 255, L"Patching file: %s", sourceFile);
			int outputLength = 0;
			u8 * outputFileBuffer;
			switch (patchType)
			{
			case 0: //PatchResult_bsdiff_Arc
				outputLength = getOutputSizeARC(true, updateFileBuffer);
				outputFileBuffer = (u8*)memalign(32, outputLength);
				if (!ApplyPatchARC(true, sourceFileBuffer, sourceLength, updateFileBuffer, updateFileBufferLength, outputFileBuffer, outputLength))
				{
					swprintf(sCurrentInfoText, 255, L"Failed to apply patch for %s", updateFile);
					sleep(2);
					free(sourceFileBuffer);
					free(updateFileBuffer);
					free(outputFileBuffer);
					goto loadNextCursor;
				}
				break;
			case 1: //PatchResult_xdelta3_Arc
				goto loadNextCursor;
			case 2: //PatchResult_bsdiff_Raw
				outputLength = getOutputSizeOther(true, updateFileBuffer);
				outputFileBuffer = (u8*)memalign(32, outputLength);
				if (!ApplyPatchOther(true, sourceFileBuffer, sourceLength, updateFileBuffer, updateFileBufferLength, outputFileBuffer, outputLength))
				{
					swprintf(sCurrentInfoText, 255, L"Failed to apply patch for %s", updateFile);
					sleep(2);
					free(sourceFileBuffer);
					free(updateFileBuffer);
					free(outputFileBuffer);
					goto loadNextCursor;
				}
				break;
			case 3: //PatchResult_xdelta3_Raw
				goto loadNextCursor;
			default:
				break;
			}
			free(sourceFileBuffer);
			free(updateFileBuffer);

			swprintf(sCurrentInfoText, 255, L"Verifying file: %s", destinationFile);
			WriteHash(outputFileBuffer, outputLength, md5Hash);
			if (strcasecmp(destinationMD5, md5Hash) != 0)
			{
				swprintf(sCurrentInfoText, 255, L"Ouptut MD5 Mismatch: %s", destinationFile);
				sleep(2);
				goto loadNextCursor;
			}
			swprintf(sCurrentInfoText, 255, L"Writing file: %s", destinationFile);

			FILE * fpDestionationFile = fopen(destinationFile, "wb");

			if (fpDestionationFile == NULL)
			{
				swprintf(sCurrentInfoText, 255, L"Could not open file for binary write %s\n", destinationFile);
				sleep(2);
				break;
			}
			fwrite(outputFileBuffer, outputLength, 1, fpDestionationFile);
			openedFiles.push_back(fpDestionationFile);
			free(outputFileBuffer);

		}
		else if (strcasecmp(patchMethod, "add") == 0)
		{
			//<file method="add" updateFile="info/portrite/InfFace127.brres" destinationFile="/codespath/pf/info/portrite/InfFace127.brres" destinationMD5="817fea2c2cae0f775ac9d3bbe3fb229c"/>
			const char * updateFile = cur->Attribute("updateFile");
			const char * destinationFile = cur->Attribute("destinationFile");
			const char * destinationMD5 = cur->Attribute("destinationMD5");

			swprintf(sCurrentInfoText, 255, L"Checking for existing file: %s", destinationFile);

			FILE * existCheckFile = fopen(destinationFile, "rb");
			if (existCheckFile != NULL)
			{
				swprintf(sCurrentInfoText, 255, L"Verifying file: %s", destinationFile);
				fseek(existCheckFile, 0, SEEK_END);
				int existingFileSize = ftell(existCheckFile);
				u8* existingFileBuffer = (u8*)memalign(32, existingFileSize);
				fseek(existCheckFile, 0, SEEK_SET);
				fread(existingFileBuffer, existingFileSize, 1, existCheckFile);
				//openedFiles.push_back(existCheckFile);
				WriteHash(existingFileBuffer, existingFileSize, md5Hash);
				free(existingFileBuffer);
				fclose(existCheckFile);
				if (strcasecmp(destinationMD5, md5Hash) == 0)
				{
					swprintf(sCurrentInfoText, 255, L"Skipping file: %s", destinationFile);
					goto loadNextCursor;
				}
			}

			swprintf(sCurrentInfoText, 255, L"Searching for file in archive: %s", updateFile);

			//searchFile isn't working...
			int fileIndex = -1;
			for (int i = 0, len = sZip.GetItemCount(); i < len; i++)
			{
				const char* filename = sZip.GetFileStruct(i)->filename;
				if (filename != NULL && strcasecmp(filename, updateFile) == 0)
				{
					fileIndex = i;
					break;
				}
			}


			ArchiveFileStruct* curArchive = sZip.GetFileStruct(fileIndex);
			if (!curArchive)
			{
				swprintf(sCurrentInfoText, 255, L"Couldn't find %s", updateFile);
				sleep(2);
				goto loadNextCursor;
			}


			swprintf(sCurrentInfoText, 255, L"Reading file from archive: %s", updateFile);

			int updateFileBufferLength = curArchive->length;
			u8 * updateFileBuffer = (u8*)memalign(32, updateFileBufferLength);
			//memset(updateFileBuffer, 0, updateFileBufferLength);

			sZip.ExtractIntoMemory(curArchive->fileindex, updateFileBuffer, updateFileBufferLength);


			swprintf(sCurrentInfoText, 255, L"Verifying file from archive: %s", updateFile);

			WriteHash(updateFileBuffer, updateFileBufferLength, md5Hash);
			if (strcasecmp(destinationMD5, md5Hash) != 0)
			{
				swprintf(sCurrentInfoText, 255, L"Output MD5 Mismatch: %s", destinationFile);
				sleep(2);
				free(updateFileBuffer);
				goto loadNextCursor;
			}

			swprintf(sCurrentInfoText, 255, L"Writing new file: %s", destinationFile);

			FILE * fpDestionationFile = fopen(destinationFile, "wb");

			if (fpDestionationFile == NULL)
			{
				swprintf(sCurrentInfoText, 255, L"Could not open file for binary write %s\n", destinationFile);
				sleep(2);
				break;
			}

			fwrite(updateFileBuffer, updateFileBufferLength, 1, fpDestionationFile);
			openedFiles.push_back(fpDestionationFile);
			free(updateFileBuffer);

		}

		//}else{swprintf(sCurrentInfoText, 255, L"Skipping Item #%d", currentResource);}
	loadNextCursor:
		swprintf(sCurrentInfoText, 255, L"Loading next file...");
		cur = cur->NextSiblingElement("file");
		currentResource++;
	}
	swprintf(sCurrentInfoText, 255, L"Closing files...");
	int openedFilesLen = openedFiles.size();
	for (int i = 0; i < openedFilesLen; i++)
	{
		fProgressPercentage = (f32)i / openedFilesLen;

		fclose(openedFiles[i]);
		openedFiles[i] = NULL;
		//openedFiles[i]->FFlush();
		//openedFiles[i]->FClose();
		//free(openedFiles[i]);
	}
	openedFiles.clear();
	fProgressPercentage = 1.0f;
	swprintf(sCurrentInfoText, 255, L"All files closed.");


	//resumeW.FClose();
	free(xmlFile);
	/* Disc system release commented out - disc loading moved to loader.dol
	if (isDiscSystemPrepared)
	{
		fProgressPercentage = 0.0f;
		swprintf(sCurrentInfoText, 255, L"Releasing disc system...");
		releaseDiscSystem();
		fProgressPercentage = 1.0f;
	}
	*/
	if (bForceCancel == true)
		return false;
	f32 value = 1.0f;


	//remove("/resume.bin");
	swprintf(sCurrentInfoText, 255, L"Patching complete!");

	return true;
}
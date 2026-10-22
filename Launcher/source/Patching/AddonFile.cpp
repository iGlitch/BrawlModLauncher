#ifdef HW_RVL
#include <memory>
#include <vector>
#include <gctypes.h>
#elif WIN32
#include <memory>
#include <vector>
#endif

#include <malloc.h>
#include <unistd.h>



#include "AddonFile.h"
#include "tinyxml2.h"
#include "../Graphics/FreeTypeGX.h"
#include "Patcher.h"
#include "../Common.h"

using namespace tinyxml2;

static inline void getStatusPath(char *out, size_t size) {
	snprintf(out, size, "%s/launcher/addons/status.xml", codesBasePath);
}

AddonFile::AddonFile(const char* filePath)
{
	FilePath = filePath;
	files.reserve(0);
	state = ADDON_FILE_STATE_UNKNOWN;
}

AddonFile::~AddonFile()
{
	for (int i = 0; i < files.size(); i++)
	if (files[i] != NULL)
		free(files[i]);
	files.clear();
	delete sZip;
}

int AddonFile::open()
{

	sZip = new SzFile(FilePath);
	u32 numItems = sZip->GetItemCount();
	if (numItems <= 0)
		return -1;

	std::vector<const char*> names;
	names.reserve(numItems);
	u8* xmlFile = NULL;
	size_t xmlFileLength = 0;

	for (int i = 0; i < (int)numItems; i++)
	{
		ArchiveFileStruct* st = sZip->GetFileStruct(i);
		names.push_back(st->filename);
		if (strcasecmp(st->filename, "info.xml") == 0)
		{
			xmlFileLength = st->length;
			xmlFile = (u8*)memalign(32, xmlFileLength);
			sZip->ExtractIntoMemory(i, xmlFile, xmlFileLength);
		}
	}
	names.clear();

	if (xmlFile == NULL || xmlFileLength == 0)
		return -2;


	XMLDocument doc;
	if (doc.Parse((const char *)xmlFile, xmlFileLength) != (int)tinyxml2::XML_NO_ERROR)
	{
		free(xmlFile);
		return -3;
	}


	XMLElement* root = doc.RootElement();
	if (!root)
	{
		free(xmlFile);
		return -4;
	}

	XMLElement* cur = root->FirstChildElement("title");
	if (!cur)
	{
		free(xmlFile);
		return -5;
	}

	if (!cur->FirstChild() || !cur->FirstChild()->ToText())
	{
		free(xmlFile);
		return -6;
	}

	sprintf(title, cur->FirstChild()->ToText()->Value());


	cur = root->FirstChildElement("code");
	if (!cur)
	{
		free(xmlFile);
		return -7;
	}

	if (!cur->FirstChild() || !cur->FirstChild()->ToText())
	{
		free(xmlFile);
		return -8;
	}

	sprintf(code, cur->FirstChild()->ToText()->Value());


	cur = root->FirstChildElement("version");
	if (!cur)
	{
		free(xmlFile);
		return -9;
	}

	if (!cur->FirstChild() || !cur->FirstChild()->ToText())
	{
		free(xmlFile);
		return -10;
	}

	version = atof(cur->FirstChild()->ToText()->Value());

	cur = root->FirstChildElement("files");
	if (!cur)
	{
		free(xmlFile);
		return -11;
	}

	cur = cur->FirstChildElement("file");

	while (cur)//patch main loop
	{
		const char * destinationFile = cur->Attribute("destination");
		if (destinationFile == NULL)
		{
			free(xmlFile);
			return -12;
		}
		char * item = malloc(strlen(destinationFile) + 1);
		strcpy(item, destinationFile);

		files.push_back(item);
		cur = cur->NextSiblingElement("file");
	}

	free(xmlFile);

	CheckState();
	return 0;

}

int AddonFile::Install(){
	
	
	//SzFile sZip(FilePath);	
	u32 numItems = sZip->GetItemCount();
	if (numItems <= 0)
		return -1;

	

	u8* xmlFile = NULL;
	size_t xmlFileLength = 0;
	for (int i = 0; i < (int)numItems; i++)
	{
		ArchiveFileStruct* st = sZip->GetFileStruct(i);
		if (strcasecmp(st->filename, "info.xml") == 0)
		{
			xmlFileLength = st->length;
			xmlFile = (u8*)memalign(32, xmlFileLength);
			sZip->ExtractIntoMemory(i, xmlFile, xmlFileLength);
		}
	}


	XMLDocument doc;
	if (doc.Parse((const char *)xmlFile, xmlFileLength) != (int)tinyxml2::XML_NO_ERROR)
	{
		free(xmlFile);
		return -3;
	}


	XMLElement* root = doc.RootElement();
	if (!root)
	{
		free(xmlFile);
		return -4;
	}


	XMLElement * cur = root->FirstChildElement("files");
	if (!cur)
	{
		free(xmlFile);
		return -5;
	}

	cur = cur->FirstChildElement("file");
	if (!cur)
	{
		free(xmlFile);
		return -6;
	}

	char md5Hash[32];
	std::vector<FILE *> openedFiles;
	int errorNumber = 0;
	while (cur)//patch main loop
	{
		const char * source = cur->Attribute("source");
		const char * destinationFile = cur->Attribute("destination");
		const char * destinationMD5 = cur->Attribute("md5");


		FILE * existCheckFile = fopen(destinationFile, "rb");
		if (existCheckFile != NULL)
		{

			fseek(existCheckFile, 0, SEEK_END);
			int existingFileSize = ftell(existCheckFile);
			u8* existingFileBuffer = (u8*)memalign(32, existingFileSize);
			fseek(existCheckFile, 0, SEEK_SET);
			fread(existingFileBuffer, existingFileSize, 1, existCheckFile);
			//openedFiles.push_back(existCheckFile);
			WriteHash(existingFileBuffer, existingFileSize, md5Hash);

			fclose(existCheckFile);
			if (strcasecmp(destinationMD5, md5Hash) == 0)
			{
				//swprintf(sCurrentInfoText, 255, L"Skipping file: %s", destinationFile);
				cur = cur->NextSiblingElement("file");
				continue;
			}
			else
			{


				char backupDir[ISFS_MAXPATH];
				sprintf(backupDir, "%s/launcher/addons/backups", codesBasePath);
				CreateSubfolder(backupDir);
				char backupLocation[ISFS_MAXPATH];
				sprintf(backupLocation, "%s/launcher/addons/backups/%s", codesBasePath, GetFileName(destinationFile));
				FILE * fpBackUpFileCheck = fopen(backupLocation, "rb");
				if (fpBackUpFileCheck == NULL)
				{
					FILE * fpBackUpFile = fopen(backupLocation, "wb");

					if (fpBackUpFile == NULL)
					{
						free(existingFileBuffer);
						free(xmlFile);
						errorNumber = -7;
						break;
					}
					else
					{
						fwrite(existingFileBuffer, existingFileSize, 1, fpBackUpFile);
						openedFiles.push_back(fpBackUpFile);
					}
				}
				else
				{
					//Found backup, meaning this will create conflict with other addon.
					fclose(fpBackUpFileCheck);
				}

			}

			free(existingFileBuffer);
		}

		//swprintf(sCurrentInfoText, 255, L"Searching for file in archive: %s", updateFile);

		//searchFile isn't working...
		int fileIndex = -1;
		for (int i = 0, len = sZip->GetItemCount(); i < len; i++)
		{			
			const char* filename = sZip->GetFileStruct(i)->filename;
			if (filename != NULL && strcasecmp(filename, source) == 0)
			{
				fileIndex = i;
				break;
			}
		}


		ArchiveFileStruct* curArchive = sZip->GetFileStruct(fileIndex);
		if (!curArchive)
		{
			//swprintf(sCurrentInfoText, 255, L"Couldn't find %s", updateFile);
			//sleep(2);			
			errorNumber = -8;
			break;
			//goto loadNextCursor;
		}



		int updateFileBufferLength = curArchive->length;
		u8 * updateFileBuffer = (u8*)memalign(32, updateFileBufferLength);
		//memset(updateFileBuffer, 0, updateFileBufferLength);

		sZip->ExtractIntoMemory(curArchive->fileindex, updateFileBuffer, updateFileBufferLength);



		WriteHash(updateFileBuffer, updateFileBufferLength, md5Hash);
		if (strcasecmp(destinationMD5, md5Hash) != 0)
		{
			free(updateFileBuffer);
			errorNumber = -9;
			break;
			//goto loadNextCursor;
		}



		FILE * fpDestionationFile = fopen(destinationFile, "wb");

		if (fpDestionationFile == NULL)
		{
			free(updateFileBuffer);

			errorNumber - 10;
			break;
		}


		if (!logChangedFile(destinationFile))
		{
			free(updateFileBuffer);

			return -11;
			break;
		}


		fwrite(updateFileBuffer, updateFileBufferLength, 1, fpDestionationFile);
		openedFiles.push_back(fpDestionationFile);

		free(updateFileBuffer);

		cur = cur->NextSiblingElement("file");

	}
	free(xmlFile);
	int openedFilesLen = openedFiles.size();
	for (int i = 0; i < openedFilesLen; i++)
	{

		fclose(openedFiles[i]);
		openedFiles[i] = NULL;
		//openedFiles[i]->FFlush();
		//openedFiles[i]->FClose();
		//free(openedFiles[i]);
	}
	openedFiles.clear();
	return errorNumber;
}


int AddonFile::Uninstall(){
	//SzFile sZip(FilePath);
	u32 numItems = sZip->GetItemCount();
	if (numItems <= 0)
		return -1;

	std::vector<const char*> names;
	names.reserve(numItems);
	u8* xmlFile = NULL;
	size_t xmlFileLength = 0;

	for (int i = 0; i < (int)numItems; i++)
	{
		ArchiveFileStruct* st = sZip->GetFileStruct(i);
		names.push_back(st->filename);
		if (strcasecmp(st->filename, "info.xml") == 0)
		{
			xmlFileLength = st->length;
			xmlFile = (u8*)memalign(32, xmlFileLength);
			sZip->ExtractIntoMemory(i, xmlFile, xmlFileLength);
		}
	}
	names.clear();

	if (xmlFile == NULL || xmlFileLength == 0)
		return -2;


	XMLDocument doc;
	if (doc.Parse((const char *)xmlFile, xmlFileLength) != (int)tinyxml2::XML_NO_ERROR)
	{
		free(xmlFile);
		return -3;
	}


	XMLElement* root = doc.RootElement();
	if (!root)
	{
		free(xmlFile);
		return -4;
	}

	XMLElement * cur = root->FirstChildElement("files");
	if (!cur)
	{
		free(xmlFile);
		return -5;
	}

	cur = cur->FirstChildElement("file");




	int currentResource = 0;
	char md5Hash[32];
	std::vector<FILE *> openedFiles;
	int errorNumber = 0;
	while (cur)//patch main loop
	{
		const char * destinationFile = cur->Attribute("destination");

		if (checkIfChanged(destinationFile) == 0)
			continue;


		char backupDir[ISFS_MAXPATH];
		sprintf(backupDir, "%s/launcher/addons/backups", codesBasePath);
		CreateSubfolder(backupDir);
		char backupLocation[ISFS_MAXPATH];
		sprintf(backupLocation, "%s/launcher/addons/backups/%s", codesBasePath, GetFileName(destinationFile));

		FILE * fpBackUpFile = fopen(backupLocation, "rb");
		if (fpBackUpFile != NULL)
		{

			fseek(fpBackUpFile, 0, SEEK_END);
			int backupFileSize = ftell(fpBackUpFile);
			u8* backupFileBuffer = (u8*)memalign(32, backupFileSize);
			fseek(fpBackUpFile, 0, SEEK_SET);
			fread(backupFileBuffer, backupFileSize, 1, fpBackUpFile);
			//openedFiles.push_back(existCheckFile);			

			fclose(fpBackUpFile);

			FILE * fpDestionationFile = fopen(destinationFile, "wb");

			if (fpDestionationFile == NULL)
			{
				free(xmlFile);
				return -7;
				break;
			}
			if (!logRestoredFile(destinationFile))
			{
				free(xmlFile);
				return -8;
				break;
			}
			fwrite(backupFileBuffer, backupFileSize, 1, fpDestionationFile);
			openedFiles.push_back(fpDestionationFile);
			free(backupFileBuffer);

			remove(backupLocation);
		}
		else
		{
			remove(destinationFile);
			//no backup!?
		}



		cur = cur->NextSiblingElement("file");

	}
	int openedFilesLen = openedFiles.size();
	for (int i = 0; i < openedFilesLen; i++)
	{

		fclose(openedFiles[i]);
		openedFiles[i] = NULL;
	}
	openedFiles.clear();
	return errorNumber;
}


bool AddonFile::logRestoredFile(const char * fileName){

	bool buildXML = false;
	tinyxml2::XMLElement* xeRoot;
	tinyxml2::XMLElement* xeFiles;

	tinyxml2::XMLDocument statusDoc;
	char statusPath[ISFS_MAXPATH];
	getStatusPath(statusPath, sizeof(statusPath));
	if (statusDoc.LoadFile(statusPath) == (int)tinyxml2::XML_NO_ERROR)
	{
		xeRoot = statusDoc.RootElement();
		if (xeRoot == NULL)
		{
			xeRoot = statusDoc.NewElement("status");
			statusDoc.InsertEndChild(xeRoot);
		}
		xeFiles = xeRoot->FirstChildElement("files");
		if (xeFiles == NULL)
		{
			xeFiles = statusDoc.NewElement("files");
			xeRoot->InsertEndChild(xeFiles);
		}
	}

	tinyxml2::XMLElement* xeFile = xeFiles->FirstChildElement("file");

	bool foundMatch = false;
	bool foundFileName = false;
	while (xeFile)
	{
		const char * _path = xeFile->Attribute("path");
		const char * _code = xeFile->Attribute("code");

		if (strcasecmp(fileName, _path) == 0)
		{
			foundFileName = true;
			xeFiles->DeleteChild(xeFile);
			break;
		}
		xeFile = xeFile->NextSiblingElement("file");
	}

	if (!foundFileName)
		return true;

	FILE * statusXML = fopen(statusPath, "wb");
	int ret = 1;
	if (statusXML != NULL)
	{
		ret = statusDoc.SaveFile(statusXML);
		fflush(statusXML);
		fclose(statusXML);
	}

	return ret == XML_NO_ERROR;
}

bool AddonFile::logChangedFile(const char * fileName){



	bool buildXML = false;
	tinyxml2::XMLElement* xeRoot;
	tinyxml2::XMLElement* xeFiles;

	tinyxml2::XMLDocument statusDoc;
	char statusPath[ISFS_MAXPATH];
	getStatusPath(statusPath, sizeof(statusPath));
	if (statusDoc.LoadFile(statusPath) == (int)tinyxml2::XML_NO_ERROR)
	{
		xeRoot = statusDoc.RootElement();
		if (xeRoot == NULL)
		{
			xeRoot = statusDoc.NewElement("status");
			statusDoc.InsertEndChild(xeRoot);
		}
		xeFiles = xeRoot->FirstChildElement("files");
		if (xeFiles == NULL)
		{
			xeFiles = statusDoc.NewElement("files");
			xeRoot->InsertEndChild(xeFiles);
		}
	}

	tinyxml2::XMLElement* xeFile = xeFiles->FirstChildElement("file");

	bool foundMatch = false;
	bool foundFileName = false;
	while (xeFile)
	{
		const char * _path = xeFile->Attribute("path");
		const char * _code = xeFile->Attribute("code");

		if (strcasecmp(fileName, _path) == 0)
		{
			foundFileName = true;
			if (strcasecmp(code, _code) == 0)
				foundMatch = true;
			else
				xeFile->SetAttribute("code", code);
			break;
		}
		xeFile = xeFile->NextSiblingElement("file");
	}
	if (foundMatch)
		return true;

	if (!foundFileName)
	{
		xeFile = statusDoc.NewElement("file");
		xeFile->SetAttribute("path", fileName);
		xeFile->SetAttribute("code", code);
		xeFiles->InsertEndChild(xeFile);
	}
	FILE * statusXML = fopen(statusPath, "wb");
	int ret = 1;
	if (statusXML != NULL)
	{
		ret = statusDoc.SaveFile(statusXML);
		fflush(statusXML);
		fclose(statusXML);
	}

	return ret == XML_NO_ERROR;
}

int AddonFile::CheckState()
{


	tinyxml2::XMLDocument statusDoc;
	char statusPath[ISFS_MAXPATH];
	getStatusPath(statusPath, sizeof(statusPath));
	if (statusDoc.LoadFile(statusPath) != (int)tinyxml2::XML_NO_ERROR)
	{
		state = ADDON_FILE_STATE_NOT_INSTALLED;
		return -2;
	}


	tinyxml2::XMLElement* xeRoot = statusDoc.RootElement();
	if (!xeRoot){
		state = ADDON_FILE_STATE_NOT_INSTALLED;
		return -3;
	}
	tinyxml2::XMLElement* xeFiles = xeRoot->FirstChildElement("files");
	if (!xeFiles){
		state = ADDON_FILE_STATE_NOT_INSTALLED;
		return -4;
	}

	tinyxml2::XMLElement* xeFile;

	bool installed = false;
	bool conflict = false;
	bool incomplete = false;
	if (files.size() == 0)
	{
		state = ADDON_FILE_STATE_NOT_INSTALLED;
		return -5;
	}
	for (int i = 0; i < files.size(); i++)
	{

		xeFile = xeFiles->FirstChildElement("file");
		bool found = false;
		while (!found && xeFile)
		{
			const char * _path = xeFile->Attribute("path");
			const char * _code = xeFile->Attribute("code");
			if (strcasecmp(files[i], _path) == 0)
			{
				found = true;
				installed = true;
				if (strcasecmp(code, _code) != 0)
					conflict = true;
			}
			else
				xeFile = xeFile->NextSiblingElement("file");
		}
		if (!found)
			incomplete = true;
	}

	if (conflict)
		state = ADDON_FILE_STATE_CONFLICTING;
	else if (installed){
		if (incomplete)
			state = ADDON_FILE_STATE_INCOMPLETE;
		else
			state = ADDON_FILE_STATE_INSTALLED;
	}
	else
		state = ADDON_FILE_STATE_NOT_INSTALLED;

	return 0;
}

int AddonFile::checkIfChanged(const char * fileName)
{

	tinyxml2::XMLDocument doc;
	char statusPath[ISFS_MAXPATH];
	getStatusPath(statusPath, sizeof(statusPath));
	if (doc.LoadFile(statusPath) != (int)tinyxml2::XML_NO_ERROR)
		return -2;



	tinyxml2::XMLElement* xeRoot = doc.RootElement();
	if (!xeRoot)
		return -3;

	tinyxml2::XMLElement* xeFiles = xeRoot->FirstChildElement("files");
	if (!xeFiles)
		return -4;

	tinyxml2::XMLElement* xeFile = xeFiles->FirstChildElement("file");


	while (xeFile)
	{
		const char * _path = xeFile->Attribute("path");
		const char * _code = xeFile->Attribute("code");

		if (strcasecmp(fileName, _path) == 0)
		{
			if (strcasecmp(code, _code) == 0)
				return 1;
			else
				return 2;
		}
		xeFile = xeFile->NextSiblingElement("file");
	}

	return 0;
}
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
#ifndef _7ZIPFILE_H_
#define _7ZIPFILE_H_

#include "ArchiveStruct.h"

extern "C" {
#include "7z.h"
#include "7zFile.h"
#include "7zAlloc.h"
#include "7zCrc.h"
}

class SzFile
{
        public:
                //!Constructor
                SzFile(const char *filepath);
                //!Destructor
                ~SzFile();
                //!Check if it is a 7zip file
                bool Is7ZipFile (const char *buffer);
                //!Get the archive file structure
                ArchiveFileStruct * GetFileStruct(int fileIndx);
                //!Extract file from a 7zip to file
                int ExtractFile(int fileindex, const char * outpath, bool withpath = false);
                //!Extract file into memory. Pointer "dst" must have enough space.
                //Return value is the size of the extracted filesize
                int ExtractIntoMemory(int fileindex,u8* dst,int dstSize);
                //!Extract all files from the 7zip to a path
                int ExtractAll(const char * destpath);
                //!Get the total amount of items inside the archive
                u32 GetItemCount();

                SRes SzResult;

        private:
                char *GetUtf8Filename(int ind);
                void DisplayError(SRes res);

                ArchiveFileStruct CurArcFile;
                CFileInStream archiveStream;
                CLookToRead lookStream;
                CSzArEx SzArchiveDb;
                ISzAlloc SzAllocImp;
                ISzAlloc SzAllocTempImp;
                UInt32 SzBlockIndex;
                CSzFileItem * SzFileItem;
};

#endif

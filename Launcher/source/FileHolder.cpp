#include "FileHolder.h"
#include <stdarg.h>

FileHolder::FileHolder(const char* paramFilename,const char* mode)
{
	_fp = NULL;
	_fp = fopen(paramFilename, mode);
	filename = paramFilename;
}

FileHolder::~FileHolder()
{
	if (_fp)
		fclose(_fp);
	_fp = NULL;
}

int FileHolder::Size()
{
	if (!_fp)return -1;
	fseek(_fp, 0, SEEK_END);
	int ret = ftell(_fp);
	fseek(_fp, 0, SEEK_SET);
	return ret;
}

int FileHolder::FSeek(long offset, int origin)
{
	return _fp?fseek(_fp, offset, origin):-1;
}

size_t FileHolder::FRead(void* buf, size_t size, size_t n)
{
	return _fp?fread(buf, size, n, _fp):-1;
}

size_t FileHolder::FWrite(const void* buf, size_t size, size_t n)
{
	return _fp?fwrite(buf, size, n, _fp):-1;
}

char* FileHolder::FGetS(char* row, int len)
{
	return _fp ? fgets(row, len, _fp) : NULL;
}

int FileHolder::FEOF()
{
	return _fp ? feof(_fp) : -1;
}

int FileHolder::FPrintF(const char* format, ...)
{
	if (!_fp)
		return -1;
	va_list arg;
	va_start(arg, format);
	int ret=vfprintf(_fp, format, arg);
	va_end(arg);
	return ret;
}

int FileHolder::FFlush()
{
	return _fp ? fflush(_fp) : -1;
}

int FileHolder::FClose()
{
	int ret = 0;
	if (_fp)
	{
		ret=fclose(_fp);
		_fp = NULL;
	}
	return ret;
}

int FileHolder::FSync()
{
	return fsync(_fp->_file);
}

bool FileHolder::IsOpen()
{
	return _fp!=NULL;
}

FILE* FileHolder::get_Raw()
{
	return _fp;
}
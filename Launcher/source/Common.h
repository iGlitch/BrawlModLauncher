#ifndef _COMMON_H_
#define _COMMON_H_

#include <string.h>
#include <gccore.h>

#ifdef WIN32
#define BREAK ;
#elif HW_RVL
#include <debug.h>
//Debugging macro
#define BREAK _break();
#endif

void loadInfoFile();

//_____Global variables_____
extern char customDolPath[ISFS_MAXPATH];
extern char codesBasePath[ISFS_MAXPATH];
extern char infoMusicPath[ISFS_MAXPATH];
extern char updateUrl[512];
extern char projectName[64];
extern char infoVersionStr[32];
extern f32 infoVersion;
extern char gameVersionStr[32];
extern f32 gameVersion;
extern GXColor selectionColor;

// Config variables from main.cpp
enum LoadMethod {
    LOAD_AUTO = 0,
    LOAD_DISC = 1,
    LOAD_USB = 2
};
extern LoadMethod loadMethod;
extern bool isBadBoot;
extern bool autoBoot;
extern bool debugToFile;

//_____Path handling_____
const char* GetFileName(const char* path);
const char* GetExtension(const char* filepath);
void GetDirectory(const char* src, char* buf);

//_____General string handling_____
bool Contains(const char* target, const char* str);
void ToLower(const char* str, char* buf);
//Declarate string
#define DECLSTR(ident) char ident[256];memset(ident,0,sizeof(ident));
//Declarate and initialize string
#define INITSTR(ident,src) char ident[256];memset(ident,0,sizeof(ident));strcpy(ident,src);
//Concatenate strings into one string
#define CATSTR(ident,first,second) char ident[256];memset(ident,0,sizeof(ident));strcpy(ident,first);strcat(ident,second);
#endif
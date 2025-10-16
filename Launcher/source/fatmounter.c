#include <fat.h>
#include <ogc/mutex.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

//these are the only stable and speed is good
#define CACHE 8
#define SECTORS 64

int USBDevice_Init()
{
        //closing all open Files write back the cache and then shutdown em!
        fatUnmount("usb:/");

    if (fatMount("usb", &__io_usbstorage, 0, CACHE, SECTORS)) {
                return 1;
        }
        return -1;
}

void USBDevice_deInit()
{
        //closing all open Files write back the cache and then shutdown em!
        fatUnmount("usb:/");
}

int USBDevice_Inserted()
{
        return __io_usbstorage.isInserted();
}

int SDCard_Inserted()
{
    return __io_wiisd.isInserted();
}

int SDCard_Init()
{
        //closing all open Files write back the cache and then shutdown em!
        fatUnmount("sd:/");
        //right now mounts first FAT-partition
        if (fatMount("sd", &__io_wiisd, 0, CACHE, SECTORS))
                return 1;
        return -1;
}

void SDCard_deInit()
{
        //closing all open Files write back the cache and then shutdown em!
        fatUnmount("sd:/");
}


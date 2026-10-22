#include <stdlib.h>
#include <math.h>
#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <ogcsys.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <dirent.h>


#include "video.h"
#include "TestScene.h"
#include "stdlib.h"
#include "FreeTypeGX.h"
#include "..\Audio\sfx.h"
#include "..\Patching\tinyxml2.h"
#include "..\Graphics\FreeTypeGX.h"
#include "textures.h"
#include "..\Patching\7z\CreateSubfolder.h"
#include "..\USB\usbadapter.h"


CTestScene::CTestScene(f32 w, f32 h)
{
	m_eNextScreen = SCENE_TEST;

	m_fScreenWidth = w;
	m_fScreenHeight = h;
	m_bIsLoaded = false;
	m_iMenuSelectionAnimationFrames = 15;
	m_fMaxNewsScrollFrames = f32(60 * 25);
	m_fCurrentNewsScrollFrame = f32(-1);


};

void CTestScene::Load()
{
	m_iMenuSelectedIndex = -2L;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);
	m_eNextScreen = SCENE_TEST;


	sInfoText = malloc(sizeof(wchar_t) * 255);
	memset(sInfoText, 0, sizeof(wchar_t) * 255);
	//addonFiles.reserve(0);

};

void CTestScene::Unload()
{
	m_iMenuSelectedIndex = -1L;
	m_iMenuSelectionFrame = 0;
	m_iDrawFrameNumber = 0;
	m_sPrevDStickX = s8(0);
	m_sPrevDStickY = s8(0);

	delete sInfoText;

};

void CTestScene::HandleInputs(u32 gcPressed, s8 dStickX, s8 dStickY, s8 cStickX, s8 cStickY, u32 wiiPressed, bool powerButtonPressed, bool resetButtonPressed)
{
	if (m_iDrawFrameNumber < 30)
		return;

	if (gcPressed & PAD_BUTTON_B || wiiPressed & WPAD_BUTTON_B || (powerButtonPressed && m_iMenuSelectedIndex == -1))
	{
		m_eNextScreen = SCENE_MAIN_MENU;
		playSFX(SFX_BACK);
	}
	return;




	m_sPrevDStickX = dStickX;
	m_sPrevDStickY = dStickY;

}

void CTestScene::Draw()
{


	f32 yPos = 46.0f;
	f32 offset = 0.0F;
	if (m_iDrawFrameNumber < 15)
		offset = 11.5f * (m_iDrawFrameNumber - 15);


	drawNewsBox(yPos, offset, 65);

	yPos += 20.0f;

	//drawSelectionMenu(yPos - 5);

	yPos += 288.0f;

	if (m_iDrawFrameNumber < 15)
		offset = ((yPos - m_fScreenHeight) / 10.0f) * (m_iDrawFrameNumber - 15);
	else
		offset = 0.0f;

	drawInfoBox(yPos + offset, 36.0F, sInfoText);

	//if (frameNumber <= 45)



	if (m_iDrawFrameNumber <= 30)
		m_iDrawFrameNumber++;

	//swprintf(sInfoText, 255, L"menu %d", m_iMenuSelectedIndex);

}




bool initd = false;
bool CTestScene::Work()
{

	if (wu_padDataSize == 0)
	{
		swprintf(sInfoText, 255, L"No Wii U Adapter Connected");
		sleep(1);
		return true;
	}

	if (UPAD_ScanPads())
		swprintf(sInfoText, 255, L"%i %i %i %i %i %i %i %i %i", wu_currentPadData[1], wu_currentPadData[2], wu_currentPadData[3], wu_currentPadData[4], wu_currentPadData[5], wu_currentPadData[6], wu_currentPadData[7], wu_currentPadData[8], wu_currentPadData[9]);
	return true;
	/*HIDHandle = IOS_Open("/dev/usb/hid", 0);
	if (HIDHandle < 0) return; //should never happen


	hidreadheap = (u8*)malloca(32, 32);
	hidreadqueue = mqueue_create(hidreadheap, 1);
	hidreadmsg = (struct ipcmessage*)malloca(sizeof(struct ipcmessage), 32);
	HIDRead_Thread = thread_create(HIDReadAlarm, NULL, ((u32*)&__hid_read_stack_addr), ((u32)(&__hid_read_stack_size)) / sizeof(u32), 0x78, 1);
	thread_continue(HIDRead_Thread);

	hidchangeheap = (u8*)malloca(32, 32);
	hidchangequeue = mqueue_create(hidchangeheap, 1);
	hidchangemsg = (struct ipcmessage*)malloca(sizeof(struct ipcmessage), 32);
	HIDChange_Thread = thread_create(HIDChangeAlarm, NULL, ((u32*)&__hid_change_stack_addr), ((u32)(&__hid_change_stack_size)) / sizeof(u32), 0x78, 1);
	thread_continue(HIDChange_Thread);

	memset32(AttachedDevices, 0, sizeof(usb_device_entry) * 32);
	IOS_IoctlAsync(HIDHandle, GetDeviceChange, NULL, 0, VirtualToPhysical(AttachedDevices), 0x180, hidchangequeue, hidchangemsg);

	memset32((void*)HID_STATUS, 0, 0x20);
	sync_after_write((void*)HID_STATUS, 0x20);

	mdelay(100);

	if (HIDHandle < 0)
	{

	swprintf(sInfoText, 255, L"Cannot open device. (Error %ld)", HIDHandle);
	sleep(2);
	return;
	}

	swprintf(sInfoText, 255, L"Hid = %ld", HIDHandle);
	sleep(2);




	char *HIDHeap = (char*)memalign(32, 0x600);
	memset(HIDHeap, 0, 0x600);

	//BootStatusError(8, 1);
	ret = IOS_Ioctl(HIDHandle, GetDeviceChange, NULL, 0, HIDHeap, 0x600);
	//BootStatusError(8, 0);
	if (ret < 0)
	{
	swprintf(sInfoText, 255, L"HID:GetDeviceChange():%ld\r\n", ret);
	sleep(2);
	free(HIDHeap);
	return false;
	}

	DeviceID = *(vu32*)(HIDHeap + 4);
	swprintf(sInfoText, 255, L"HID:DeviceID:%u\r\n", DeviceID);
	swprintf(sInfoText, 255, L"HID:VID:%04X PID:%04X\r\n", *(vu16*)(HIDHeap + 0x10), *(vu16*)(HIDHeap + 0x12));
	sleep(2);

	u32 Offset = 8;

	u32 DeviceDescLength = *(vu8*)(HIDHeap + Offset);
	Offset += (DeviceDescLength + 3)&(~3);

	u32 ConfigurationLength = *(vu8*)(HIDHeap + Offset);
	Offset += (ConfigurationLength + 3)&(~3);

	u32 InterfaceDescLength = *(vu8*)(HIDHeap + Offset);
	Offset += (InterfaceDescLength + 3)&(~3);

	u32 EndpointDescLengthO = *(vu8*)(HIDHeap + Offset);

	bEndpointAddress = *(vu8*)(HIDHeap + Offset + 2);

	if ((bEndpointAddress & 0xF0) != 0x80)
	{
	bEndpointAddressOut = bEndpointAddress;
	Offset += (EndpointDescLengthO + 3)&(~3);
	}
	bEndpointAddress = *(vu8*)(HIDHeap + Offset + 2);
	wMaxPacketSize = *(vu16*)(HIDHeap + Offset + 4);

	swprintf(sInfoText, 255, L"HID:bEndpointAddress:%02X\r\n", bEndpointAddress);
	swprintf(sInfoText, 255, L"HID:wMaxPacketSize  :%u\r\n", wMaxPacketSize);

	free(HIDHeap);

	IOS_Close(HIDHandle);

	Packet = (u8*)memalign(32, MemPacketSize);
	memset(Packet, 0, MemPacketSize);
	//sync_after_write(Packet, MemPacketSize);

	memset(HID_Packet, 0, MemPacketSize);
	//sync_after_write(HID_Packet, MemPacketSize);
	sleep(5);
	return 0;


	//check for install request
	//install

	//check for uninstall request
	//uninstall*/

	return false;
}


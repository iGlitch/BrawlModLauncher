#include <gccore.h>
#include <ogcsys.h>
#include <unistd.h>
#include <network.h>
#include <ogcsys.h>
#include <locale.h>
#include <ogc/isfs.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <network.h>
#include <ogcsys.h>
#include <string.h>
#include <wchar.h>

#include "http.h"
#include "..\Patching\tinyxml2.h"
#include "..\IOSLoader\sys.h"

static lwp_t networkThread = LWP_THREAD_NULL;
void * networkThreadFunction();
wchar_t * newsText;
bool networkInit = false;
bool _networkOK = false;
bool _netInitComplete = false;
bool netBusy = false;

s32 _networkComplete(s32 ok, void *usrData)
{
	_networkOK = ok == 0;
	_netInitComplete = true;
	return 0;
}

bool networkThreadCancelRequested = false;
bool networkThreadComplete = false;
bool Network_Init()
{
	newsText = (wchar_t*)malloc(sizeof(wchar_t)* 4096);
	return true;
}

void Network_Start()
{
	LWP_CreateThread(&networkThread, (void *)networkThreadFunction, NULL, NULL, 0, 16);
	LWP_ResumeThread(networkThread);
}

void SleepDuringWork(int milliseconds)
{
	for (int i = 0; i < milliseconds; i++)
	{
		usleep(1000);
		if (networkThreadCancelRequested)
			return;
	}
}

void * networkThreadFunction()
{
	bool firstWiFiAttempt = true;
	while (!networkThreadCancelRequested)
	{
		while (netBusy && !networkThreadCancelRequested)
			usleep(5000);
		if (networkThreadCancelRequested)
			break;
		
		netBusy = true;
		swprintf(newsText, 4096, L"Connecting to WiFi...");

		bool noWifiAlerted = false;
		while (net_get_status() != 0 && !networkThreadCancelRequested)
		{
			_netInitComplete = false;
			while (net_get_status() == -EBUSY) 
				SleepDuringWork(5000);
			net_init_async(_networkComplete, NULL);

			
			while (!_netInitComplete && !networkThreadCancelRequested)
				SleepDuringWork(5000);

			if (_networkOK)
				break;
			else
			{
				while (net_get_status() == -EBUSY) 
					SleepDuringWork(5000);
				SleepDuringWork(1000);
				net_deinit();
				SleepDuringWork(1000);
				net_wc24cleanup();
				SleepDuringWork(1000);
				if (!noWifiAlerted)
				{
					swprintf(newsText, 4096, L"Connect your Wii to the internet to receive the latest news and updates.");
					noWifiAlerted = true;
				}
				
			}
			SleepDuringWork(5000);
		}
		netBusy = false;

		if (networkThreadCancelRequested)
			break;

		int bufferSize = 0;
		char * buffer;
		f32 progress;
		swprintf(newsText, 4096, L"Downloading news...");
		char link[] = "http://legacy.glitchery.jp/projplus/updater/update.xml"; 
		bufferSize = downloadFileToBuffer(link, &buffer, NULL, networkThreadCancelRequested, progress);

		if (bufferSize > 0)
		{
			bool failed = false;
			tinyxml2::XMLDocument doc;


			if (doc.Parse(buffer, bufferSize) != (int)tinyxml2::XML_NO_ERROR)
			{
				failed = true;
			}
			tinyxml2::XMLElement* cur = doc.RootElement();


			if (!cur)
			{
				failed = true;
				swprintf(newsText, 4096, L"Failed to parse news...");
			}
			else
			{
				cur = cur->FirstChildElement("news");
				if (!cur) 
				{
					failed = true;
					swprintf(newsText, 4096, L"Failed to parse news...");
				}
				else
				{
					if (cur->FirstChild() && cur->FirstChild()->ToText())
					{
						char * text = cur->FirstChild()->ToText()->Value();
						swprintf(newsText, 4096, L"%s", text);
					}
					else
					{
						failed = true;
						swprintf(newsText, 4096, L"No news at this time...");
					}
				}
			}
			if (failed)
				SleepDuringWork(5 * 60 * 1000);
				//SleepDuringWork(5000);
			else
				SleepDuringWork(5 * 60 * 1000);
		}
		else
		{
			swprintf(newsText, 4096, L"Failed to download news...");
			sleep(5);
		}
		free(buffer);

	}
	while (net_get_status() == -EBUSY)
		usleep(50);
	net_deinit();
	net_wc24cleanup();
	networkThreadComplete = true;
	return nullptr;
}


void Network_Stop()
{
	networkThreadCancelRequested = true;
	while (!networkThreadComplete)
		usleep(1000);
	free(newsText);
}


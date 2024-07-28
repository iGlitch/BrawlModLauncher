#include "http.h"
#include <errno.h>
#include <network.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <wchar.h>
#include "networkloader.h"

bool _httpNetworkOK = false;
bool _httpNetInitComplete = false;
extern bool netBusy;
extern f32 g_LauncherVersion;

s32 _httpNetworkComplete(s32 ret, void *usrData)
{
	_httpNetworkOK = (ret == 0);
	_httpNetInitComplete = true;
	return 0;
}

int downloadFileToBuffer(char * url, char ** buffer, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{
	if (sCurrentInfoText)
		swprintf(sCurrentInfoText, 255, L"Connecting to WiFi...");
	fProgressPercentage = 0.0f;

	while (netBusy&& !bForceCancel)
		usleep(1000);
	if (bForceCancel)
		return -1;
	netBusy = true;

	while (net_get_status() != 0 && !bForceCancel)
	{
		_httpNetInitComplete = false;
		while (net_get_status() == -EBUSY) usleep(50);
		net_init_async(_httpNetworkComplete, NULL);

		while (!_httpNetInitComplete && !bForceCancel)
		{
			usleep(1000);
		}

		if (!_httpNetworkOK)
		{
			while (net_get_status() == -EBUSY) usleep(50);
			net_deinit();
			net_wc24cleanup();
			sleep(2);
		}
		else
		{
			break;
		}
	}

	if (bForceCancel)
	{
		swprintf(sCurrentInfoText, 255, L"Cancelling...");
		netBusy = false;
		return -1;
	}

	s32 ret;
	//Create Socket for sending data
	s32 sockfdd = net_socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sockfdd < 0)
	{
		while (net_get_status() == -EBUSY) usleep(50);
		net_deinit();
		netBusy = false;
		return-1;
	}

	//Check if the url starts with "http://", if not it is not considered a valid url
	if (strncmp(url, "http://", strlen("http://")) != 0) {
		netBusy = false;
		return -1;
	}

	//Locate the path part of the url by searching for '/' past "http://"
	char *path = strchr(url + strlen("http://"), '/');
	if (path == NULL) {
		netBusy = false;
		return -1;
	}

	//Extract the domain part out of the url
	int domainlength = path - url - strlen("http://");
	if (domainlength == 0) {
		netBusy = false;
		return -1;
	}

	char domain[domainlength + 1];
	strncpy(domain, url + strlen("http://"), domainlength);
	domain[domainlength] = '\0';


	struct hostent *hostEntry;
	hostEntry = net_gethostbyname(domain);//Get ip from Server by hostname 
	if (!hostEntry){
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = PF_INET;

	memcpy((char *)&addr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

	addr.sin_port = htons(80); //Set Port
	if (sCurrentInfoText)
		swprintf(sCurrentInfoText, 255, L"Connecting to server...");
	ret = net_connect(sockfdd, (struct sockaddr *)&addr, sizeof(struct sockaddr));

	if (ret < 0)
	{
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}
	char * headerformat = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: PMLauncher %4.2f\r\n\r\n";;

	char strHeader[strlen(headerformat) + strlen(domain) + strlen(path)];
	char *r = strHeader;
	sprintf(r, headerformat, path, domain, g_LauncherVersion);
	//delete headerformat;

	if (sCurrentInfoText)
		swprintf(sCurrentInfoText, 255, L"Requesting file...");

	int bytessend = net_send(sockfdd, r, strlen(r), 0);

	if (bytessend < 0)
	{
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}

	//memset(strHeader, 0, iBodySize);
	if (sCurrentInfoText)
		swprintf(sCurrentInfoText, 255, L"Downloading file information...");

	u32 cnt = 0;
	s32 count = 0;
	s32 lastBytesReceivedCount = 0;
	char * downloadBuffer;
	int downloadBufferSize = 4096;
	downloadBuffer = malloc(downloadBufferSize);
	s32 bytesRead = s32(0);
	s32 bytesWritten = s32(0);
	
	bytesRead = lastBytesReceivedCount = net_read(sockfdd, downloadBuffer, downloadBufferSize);

	char *ptr = NULL;

	while (!(ptr = strstr(downloadBuffer, "\r\n\r\n")))
	{
		while (net_get_status() == -EBUSY)
			usleep(50);
		lastBytesReceivedCount = net_read(sockfdd, downloadBuffer, downloadBufferSize);
		if (bForceCancel || lastBytesReceivedCount < 0)
		{
			free(downloadBuffer);
			net_close(sockfdd);//cleanup
			netBusy = false;
			return -1;
		}
		bytesRead += lastBytesReceivedCount;

	}

	if (bytesRead == 0)
	{
		free(downloadBuffer);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}


	if (!strstr(downloadBuffer, "HTTP/1.1 200 OK"))
	{
		free(downloadBuffer);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}

	ptr = strcasestr(downloadBuffer, "content-length:");
	if (!ptr) 
	{
		return -1;
	}

	// ensure chars match case we expect
	for (int i = 0; i < 15; i++) {
		ptr[i] = tolower(ptr[i]);
	}
	u32 filesize;
	sscanf(ptr, "content-length: %u", &filesize);

	if (sCurrentInfoText)
		swprintf(sCurrentInfoText, 255, L"Downloading update... (0/%uKB)", filesize / 1024);
	fProgressPercentage = 0.0f;


	s32 it = s32(0);
	void *tmpBuffer = malloc(filesize);
	ptr = strstr(downloadBuffer, "\r\n\r\n");
	if (ptr != NULL)
		ptr += 4;
	s32 bytesToWrite = lastBytesReceivedCount - (ptr - downloadBuffer);

	memcpy(tmpBuffer, ptr, bytesToWrite);
	bytesWritten = bytesToWrite;
	while (bytesWritten < filesize)
	{
		if (bForceCancel)
		{
			free(tmpBuffer);
			free(downloadBuffer);
			net_close(sockfdd);//cleanup
			netBusy = false;
			return -1;
		}
		
		while (net_get_status() == -EBUSY)
			usleep(50);
		it = net_read(sockfdd, downloadBuffer, downloadBufferSize);
		if (it < 0 || net_get_status() != 0)
		{
			free(tmpBuffer);
			free(downloadBuffer);
			net_close(sockfdd);//cleanup
			netBusy = false;
			return -1;
		}
		memcpy(tmpBuffer + bytesWritten, downloadBuffer, it);
		bytesWritten += it;
		if (sCurrentInfoText)
			swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten / 1024, filesize / 1024);
		fProgressPercentage = (f32)bytesWritten / (f32)filesize;

	}
	fProgressPercentage = 1.0f;
	net_close(sockfdd);//cleanup
	free(downloadBuffer);
	*buffer = (char *)tmpBuffer;
	netBusy = false;
	return filesize;
}

bool downloadFileToDisk(char * url, char*out, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{
	swprintf(sCurrentInfoText, 255, L"Connecting to WiFi...");
	fProgressPercentage = 0.0f;

	while (netBusy&& !bForceCancel)
		usleep(1000);
	if (bForceCancel)
		return false;
	netBusy = true;

	while (net_get_status() != 0 && !bForceCancel)
	{
		_httpNetInitComplete = false;
		while (net_get_status() == -EBUSY) usleep(50);
		net_init_async(_httpNetworkComplete, NULL);

		while (!_httpNetInitComplete && !bForceCancel)
		{
			usleep(1000);
		}

		if (!_httpNetworkOK)
		{
			while (net_get_status() == -EBUSY) usleep(50);
			net_deinit();
			net_wc24cleanup();
			sleep(2);
		}
		else
		{
			break;
		}
	}

	if (bForceCancel)
	{
		swprintf(sCurrentInfoText, 255, L"Cancelling...");
		netBusy = false;
		return false;
	}


	s32 ret;
	//Create Socket for sending data
	s32 sockfdd = net_socket(PF_INET, SOCK_STREAM, IPPROTO_IP);

	

	if (sockfdd < 0)
	{
		while (net_get_status() == -EBUSY) usleep(50);
		net_deinit();
		netBusy = false;
		return false;
	}

	//Check if the url starts with "http://", if not it is not considered a valid url
	if (strncmp(url, "http://", strlen("http://")) != 0)
	{
		netBusy = false;
		return false;
	}

	//Locate the path part of the url by searching for '/' past "http://"
	char *path = strchr(url + strlen("http://"), '/');
	if (path == NULL)
	{
		netBusy = false;
		return false;
	}

	//Extract the domain part out of the url
	int domainlength = path - url - strlen("http://");
	if (domainlength == 0) {
		netBusy = false;
		return false;
	}
	char domain[domainlength + 1];
	strncpy(domain, url + strlen("http://"), domainlength);
	domain[domainlength] = '\0';


	struct hostent *hostEntry;
	hostEntry = net_gethostbyname(domain);//Get ip from Server by hostname 
	if (!hostEntry){
		net_close(sockfdd);//cleanup
		netBusy = false;
		return -1;
	}
	struct sockaddr_in addr;
	addr.sin_family = PF_INET;

	memcpy((char *)&addr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

	addr.sin_port = htons(80); //Set Port

	swprintf(sCurrentInfoText, 255, L"Connecting to server...");
	ret = net_connect(sockfdd, (struct sockaddr *)&addr, sizeof(struct sockaddr));

	if (ret < 0)
	{
		net_close(sockfdd);
		netBusy = false;
		return false;
	}

	char * headerformat = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: PMLauncher %4.2f\r\n\r\n";;

	char strHeader[strlen(headerformat) + strlen(domain) + strlen(path)];
	char *r = strHeader;
	sprintf(r, headerformat, path, domain, g_LauncherVersion);
	//delete headerformat;


	swprintf(sCurrentInfoText, 255, L"Requesting file...");

	int bytessend = net_send(sockfdd, r, strlen(r), 0);

	if (bytessend < 0)
	{
		netBusy = false;
		return false;
	}

	//memset(strHeader, 0, iBodySize);
	swprintf(sCurrentInfoText, 255, L"Downloading file information...");

	u32 cnt = 0;
	s32 count = 0;
	s32 lastBytesReceivedCount = 0;
	int downloadBufferSize = 2048;
	char *downloadBuffer = (char *)malloc(downloadBufferSize);
	s32 bytesRead = s32(0);
	s32 bytesWritten = s32(0);
	while (net_get_status() == -EBUSY)
		usleep(50);
	bytesRead = lastBytesReceivedCount = net_read(sockfdd, downloadBuffer, downloadBufferSize);

	char *ptr = NULL;

	while (!(ptr = strstr(downloadBuffer, "\r\n\r\n")))
	{
		
		while (net_get_status() == -EBUSY)
			usleep(50);
		lastBytesReceivedCount = net_read(sockfdd, downloadBuffer, downloadBufferSize);
		
		if (bForceCancel || lastBytesReceivedCount < 0)
		{
			free(downloadBuffer);
			net_close(sockfdd);//cleanup
			netBusy = false;
			return false;
		}
		bytesRead += lastBytesReceivedCount;

	}

	if (bytesRead == 0)
	{
		free(downloadBuffer);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return false;
	}


	if (!strstr(downloadBuffer, "HTTP/1.1 200 OK"))
	{
		free(downloadBuffer);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return false;
	}

	ptr = strcasestr(downloadBuffer, "content-length:");
	if (!ptr) 
	{
		return -1;
	}

	// ensure chars match case we expect
	for (int i = 0; i < 15; i++) {
		ptr[i] = tolower(ptr[i]);
	}
	u32 filesize;
	sscanf(ptr, "content-length: %u", &filesize);

	if (filesize <= 0)
	{
		free(downloadBuffer);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return false;
	}

	swprintf(sCurrentInfoText, 255, L"Downloading update... (0/%uKB)", filesize / 1024);
	fProgressPercentage = 0.0f;


	s32 it = s32(0);
	FILE *fDownload = fopen(out, "wb");
	ptr = strstr(downloadBuffer, "\r\n\r\n");
	if (ptr != NULL)
		ptr += 4;
	s32 bytesToWrite = lastBytesReceivedCount - (ptr - downloadBuffer);

	if (fwrite(ptr, 1, bytesToWrite, fDownload) != bytesToWrite)
	{
		free(downloadBuffer);
		fclose(fDownload);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return false;
	}
	
	bytesWritten = bytesToWrite;
	bool failed = false;	
	while (bytesWritten < filesize)
	{
		netBusy = true;
		memset(downloadBuffer, 0, downloadBufferSize);
		if (failed || bForceCancel)
			break;
		
		swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten / 1024, filesize / 1024);
		
		while (net_get_status() == -EBUSY) 
			usleep(50);
		
		struct pollsd sds;
		sds.socket = sockfdd;
		sds.events = POLLIN;
		sds.revents = 0;
		int ret = net_poll(&sds, 1, 60 * 1000);
		if (ret != 1)
		{
			swprintf(sCurrentInfoText, 255, L"Connection timed out.");
			sleep(5);
			failed = true;
			break;
		}

		it = net_read(sockfdd, downloadBuffer, downloadBufferSize);

		if ((it == 0))
		{
			swprintf(sCurrentInfoText, 255, L"Reconnecting... (%d/%d KB)", bytesWritten / 1024, filesize / 1024);
			sleep(5);
			failed = true;
			break;
			
		}

		
		if (it < 0)
		{
			swprintf(sCurrentInfoText, 255, L"Failed: %d", it);
			usleep(1000);
			failed= true;
			break;
		}		
		failed = (it != fwrite(downloadBuffer, 1, it, fDownload));
		if (failed)
			break;

		failed = (fsync(fDownload) == 0);
		if (failed)
			break;


		bytesWritten += it;
		swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten / 1024, filesize / 1024);
		fProgressPercentage = (f32)bytesWritten / (f32)filesize;
		
	}
	if (failed)
	{
		free(downloadBuffer);
		fclose(fDownload);
		net_close(sockfdd);//cleanup
		netBusy = false;
		return false;
	}

	fProgressPercentage = 1.0f;

	fsync(fDownload);
	fclose(fDownload);
	net_close(sockfdd);//cleanup
	free(downloadBuffer);
	netBusy = false;
	return true;
}

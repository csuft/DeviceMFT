#ifndef UVCXUAPI_H
#define UVCXUAPI_H

#include "stdafx.h"
#include "common.h"

#define INDEX_FIELD_HEAD_LEN 16
#define INDEX_INSTA_DATA_PANOOFFSET  4
#define INDEX_INSTA_DATA_UUID  5
#define INDEX_INSTA_DATA_SERIALNO  6
#define INDEX_INSTA_DATA_ACTIVATION_INFO 7
#define INDEX_INSTA_DATA_APP_DATA 8

static const UINT32 TAG_PANOOFFSET = 'PnOt';
static const UINT32 TAG_UUID = 'UuiD';
static const UINT32 TAG_SERIALNO = 'sErN';
static const UINT32 TAG_LAYOUT_VERSIO = 'LyuT';
static const UINT32 TAG_ACTIVATION_INFO = 'aTvT';
static const UINT32 TAG_APP_DATA = 'ApDt';

namespace dshow{

	typedef void* UvcDeviceHandle;
	class DirectShowControl
	{
	public:
		DirectShowControl();

		~DirectShowControl();
		/**
		*must prepare first
		*data不需要手动释放
		*/
		HRESULT UvcXuGet(char** data, UINT32 tag, int index);

		HRESULT GetDevicePath(UINT32 Vid, UINT32 Pid, std::vector<std::string> &newDevicePath, std::vector<int> &deviceNumber, std::vector<std::string> &globalDevicePath);

		HRESULT FindDeviceByPath(UvcDeviceHandle *devHandle, std::string devicePath);

		/**
		*must prepare first
		*/
		HRESULT UvcXuReadVersion();
		HRESULT ReadBuildDate(char build_date[]);
		char* GetReleaseName();

		HRESULT Prepare(UvcDeviceHandle devHandle, const GUID *xuGuid);

		/**
		*must prepare first
		*use struct VideoProcAmpProperty to choose which property to set
		*flags 0x1 : VideoProcAmpFlags::VideoProcAmp_Flags_Auto
		*flags 0x2 : VideoProcAmpFlags::VideoProcAmp_Flags_Manual
		*/
		HRESULT VideoPropSet(long Property, long lValue, long Flags);

		/**
		*must prepare first
		*use struct VideoProcAmpProperty to choose which property to Get
		*flags 0x1 : VideoProcAmpFlags::VideoProcAmp_Flags_Auto
		*flags 0x2 : VideoProcAmpFlags::VideoProcAmp_Flags_Manual
		*/
		HRESULT VideoPropGet(long Property, long *lValue, long *Flags);

		void close();
		std::vector<std::string> getDevPath();

	private:
		std::map<std::string, UvcDeviceHandle> devMap_;
		std::vector<std::string> devPath_;
		BYTE * readData_ = nullptr;
		char* releaseName_ = nullptr;
		IKsControl *pKsControl_ = nullptr;
		IAMVideoProcAmp *pAMVideoProcAmp_ = nullptr;
		DWORD nodeId_ = 0;
		GUID xuGUID_;

		UINT16 crc16(UINT8 *data_p, int length);
		HRESULT StartIO(int index, int len, ULONG &ulBytesReturned);
		HRESULT CloseIO(ULONG &ulBytesReturned);
		HRESULT UvcProperty(ULONG propertyId, ULONG flags, LPVOID data, ULONG dataLength, ULONG &ulBytesReturned);
	};
}
#endif
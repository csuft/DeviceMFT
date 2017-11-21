#ifndef UVCXUAPI_H
#define UVCXUAPI_H

#include <vidcap.h>
#include <ksmedia.h>
#include <ks.h>
#include <ksproxy.h>
#include <assert.h>
#include <uuids.h>
#include <control.h>
#include <dshow.h>
#include <map>
#include <vector>

static const GUID Insta360Exu1Guid = { 0xFAF1672D, 0xB71B, 0x4793, { 0x8C, 0x91, 0x7b, 0x1c, 0x9b, 0x7f, 0x95, 0xf8 } };

namespace uvcxu{

	typedef void* UvcDeviceHandle;
	
	class UvcXuControl
	{
	public:
		__declspec(dllexport) UvcXuControl();
		__declspec(dllexport) ~UvcXuControl();
		__declspec(dllexport) HRESULT UvcXuGet(UvcDeviceHandle devHandle, const GUID *xuGuid, char** data);//data不需要手动释放
		//HRESULT UvcXuSet(UvcDeviceHandle devHandle, GUID *xuGuid, char** data);
		__declspec(dllexport) HRESULT UvcXuGetDevicePath(UINT32 Vid, UINT32 Pid, std::vector<BSTR> &devicePath, std::vector<int> &deviceNumber);
		__declspec(dllexport) HRESULT UvcXuFindDeviceByPath(UvcDeviceHandle *devHandle, BSTR devicePath);

	private:
		std::map<BSTR, UvcDeviceHandle> devMap_;
		BYTE * readData_ = nullptr;

		UINT16 crc16(UINT8 *data_p, int length);
		HRESULT startIO(IKsControl* pKsControl, const GUID *xuGuid, DWORD nodeId, ULONG &ulBytesReturned);
		HRESULT closeIO(IKsControl* pKsControl, const GUID *xuGuid, DWORD nodeId, ULONG &ulBytesReturned);
	};
}

#endif
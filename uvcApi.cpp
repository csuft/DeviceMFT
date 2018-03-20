#include "uvcApi.h"

#define CAMERA_FW_VERSION_IO_START_CS  0x1
#define CAMERA_FW_VERSION_IO_READ_CS   0x2
#define CAMERA_FW_VERSION_RELEASE_NAME_IO_NAME  11
#define UVC_XU_IO_SET   0x0E
#define UVC_XU_IO_GET   0x0B
#define UVCPROPERTY_SET_FLAGS (KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY)
#define UVCPROPERTY_GET_FLAGS (KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY)

#pragma comment(lib, "strmiids.lib")

namespace dshow{
	DirectShowControl::DirectShowControl() :xuGUID_({ 0 })
	{
		if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
		{
			printf("CoInitialize Failed!\r\n");
			exit(1);
		}
	}

	DirectShowControl::~DirectShowControl()
	{
		if (readData_)
		{
			delete[] readData_;
			readData_ = nullptr;
		}
		if (releaseName_)
		{
			delete[] releaseName_;
			releaseName_ = nullptr;
		}

		for (int i = 0; i < devPath_.size(); i++)
		{
			if (devMap_[devPath_[i]])
			{
				((IBaseFilter*)devMap_[devPath_[i]])->Release();
				devMap_[devPath_[i]] = nullptr;
			}
		}
		devMap_.clear();
		CoUninitialize();
	}

	void DirectShowControl::close()
	{
		SAFE_RELEASE(pKsControl_);
		SAFE_RELEASE(pAMVideoProcAmp_);
	}

	HRESULT DirectShowControl::GetDevicePath(UINT32 Vid, UINT32 Pid, std::vector<std::string> &newDevicePath, std::vector<int> &deviceNumber, std::vector<std::string> &globalDevicePath)
	{
		HRESULT hr = S_OK;
		IMoniker* pMoniker = NULL;
		ICreateDevEnum *pDevEnum = NULL;
		IEnumMoniker *pClassEnum = NULL;
		wchar_t vid[5] = { 0 };
		wchar_t pid[5] = { 0 };

		// Create the system device enumerator
		hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
			IID_ICreateDevEnum, (void **)&pDevEnum);
		// Create an enumerator for the video capture devices

		if (SUCCEEDED(hr))
		{
			hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
		}

		if (SUCCEEDED(hr) && pClassEnum == NULL)
		{
			// If there are no enumerators for the requested type, then 
			// CreateClassEnumerator will succeed, but pClassEnum will be NULL.
			hr = E_FAIL;
		}
		std::vector<int> newCameraIndex;
		if (SUCCEEDED(hr))
		{
			newDevicePath.clear();
			for (int i = 0; i < devPath_.size(); i++)
			{
				if (devMap_[devPath_[i]])
				{
					((IBaseFilter*)devMap_[devPath_[i]])->Release();
					devMap_[devPath_[i]] = nullptr;
				}
			}
			devPath_.clear();
			devMap_.clear();
			deviceNumber.clear();
			int number = 0;
			while (pClassEnum->Next(1, &pMoniker, NULL) == S_OK){
				IPropertyBag *pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
				if (FAILED(hr))
				{
					pMoniker->Release();
					continue;
				}
				VARIANT var;
				VariantInit(&var);
				hr = pPropBag->Read(L"DevicePath", &var, 0);
				if (SUCCEEDED(hr))
				{
					int i = 0;
					while (wcsncmp(&var.bstrVal[i], L"vid_", 4) && i<wcslen(var.bstrVal) - 12){
						i++;
						continue;
					}
					_bstr_t t = var.bstrVal;
					char *path = (char*)t;
					if (i < wcslen(var.bstrVal) - 12 && !wcsncmp(&var.bstrVal[i + 4], vid, 4) && !wcsncmp(&var.bstrVal[i + 13], pid, 4))
					{
						//printf("%S\n", var.bstrVal)
						int index = 0;
						bool finded = false;
						if (globalDevicePath.size()>0)
						{
							while (index < globalDevicePath.size())
							{
								if (!strcmp(path, globalDevicePath[index].c_str()))
								{
									finded = true;
									break;
								}
								index++;
							}
						}
						if (!finded)
						{
							//Bind Moniker to a filter object
							IBaseFilter * pBaseFilter = NULL;
							hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pBaseFilter);
							globalDevicePath.push_back(std::string(path));
							newDevicePath.push_back(std::string(path));
							devMap_.insert(std::pair<std::string, UvcDeviceHandle>(std::string(path), pBaseFilter));
							newCameraIndex.push_back(number);
						}
						else
							deviceNumber.push_back(number);
						devPath_.push_back(std::string(path));
					}
					number++;
					VariantClear(&var);
					pPropBag->Release();
					continue;
				}
				number++;
				pPropBag->Release();
			}
		}
		for (int x = 0; x < newCameraIndex.size(); x++)
			deviceNumber.push_back(newCameraIndex[x]);
		SAFE_RELEASE(pMoniker);
		SAFE_RELEASE(pDevEnum);
		SAFE_RELEASE(pClassEnum);
		return hr;
	}

	HRESULT DirectShowControl::FindDeviceByPath(UvcDeviceHandle *devHandle, std::string devicePath)
	{
		if (!devHandle || devMap_.empty() || !devMap_[devicePath])
		{
			return E_POINTER;
		}
		*devHandle = devMap_[devicePath];
		((IBaseFilter*)*devHandle)->AddRef();
		((IBaseFilter*)devMap_[devicePath])->Release();
		devMap_[devicePath] = nullptr;
		return S_OK;
	}

	HRESULT DirectShowControl::Prepare(UvcDeviceHandle devHandle, const GUID *xuGuid)
	{
		IKsTopologyInfo *pKsTopologyInfo = nullptr;
		HRESULT hr = S_OK;
		IBaseFilter *pUnkOuter = (IBaseFilter*)devHandle;

		hr = pUnkOuter->QueryInterface(__uuidof(IKsTopologyInfo),
			(void **)&pKsTopologyInfo);
		if (!SUCCEEDED(hr))
		{
			printf("Unable to obtain IKsTopologyInfo %x\n", hr);
			return -1;
		}
		SAFE_RELEASE(pUnkOuter);
		DWORD numberOfNodes;
		hr = pKsTopologyInfo->get_NumNodes(&numberOfNodes);
		DWORD i;
		//search for IVideoProcAmp Node
		for (i = 0; i < numberOfNodes; i++)
		{
			GUID nodeGuid;
			//KSNODETYPE_VIDEO_PROCESSING用来设置亮度、白平衡之类的属性node iid IID_IVideoProcAmp
			if (SUCCEEDED(pKsTopologyInfo->get_NodeType(i, &nodeGuid)) && IsEqualGUID(nodeGuid, KSNODETYPE_VIDEO_PROCESSING))
			{
				SAFE_RELEASE(pAMVideoProcAmp_);
				hr = pKsTopologyInfo->CreateNodeInstance(i, __uuidof(IAMVideoProcAmp), (VOID**)&pAMVideoProcAmp_);
				if (SUCCEEDED(hr))
					break;

			}
		}
		//search for IKsControl Node
		for (i = 0; i < numberOfNodes; i++)
		{
			GUID nodeGuid;
			if (SUCCEEDED(pKsTopologyInfo->get_NodeType(i, &nodeGuid)) && IsEqualGUID(nodeGuid, KSNODETYPE_DEV_SPECIFIC))
			{
				// create node instance
				SAFE_RELEASE(pKsControl_);
				hr = pKsTopologyInfo->CreateNodeInstance(i, __uuidof(IKsControl), (VOID**)&pKsControl_);
				if (FAILED(hr))
					continue;
				ULONG ulBytesReturned;
				xuGUID_ = { 0 };
				xuGUID_ = *xuGuid;
				nodeId_ = i;
				if ((hr = StartIO(INDEX_INSTA_DATA_PANOOFFSET, 32, ulBytesReturned)) != S_OK)
				{
					SAFE_RELEASE(pKsControl_);
					continue;
				}
				hr = CloseIO(ulBytesReturned);
				break;
			}
		}
		SAFE_RELEASE(pKsTopologyInfo);
		return hr;
	}

	HRESULT DirectShowControl::UvcXuGet(char** data, UINT32 Tag, int xu_index)
	{
		//read data
		if (readData_)
		{
			delete[] readData_;
			readData_ = nullptr;
		}
		if (!pKsControl_)
			return E_FAIL;
		HRESULT hr = S_OK;
		ULONG ulBytesReturned = 0;
		if ((hr = StartIO(xu_index, 32, ulBytesReturned)) != S_OK)
		{
			printf("start io failed!");
			return hr;
		}
		BYTE header[32] = { 0 };
		hr = UvcProperty(0x0B, UVCPROPERTY_GET_FLAGS, &header, sizeof(header), ulBytesReturned);
		if (FAILED(hr))
		{
			printf("read header failed!");
			return hr;
		}

		UINT32 tag = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
		UINT16 checksum = header[4] | (header[5] << 8);
		UINT32 dataLen = header[6] | (header[7] << 8) | (header[8] << 16) | (header[9] << 24);
		UINT32 len = (dataLen + 31) / 32 * 32;
		if (tag != Tag || dataLen > 20 * 1024 * 1024)
		{
			hr = E_FAIL;
			printf("header tag unsatisfied!");
			return hr;
		}
		hr = CloseIO(ulBytesReturned);
		hr = StartIO(xu_index, len, ulBytesReturned);

		UINT32 readSize = 0;
		readData_ = new BYTE[len + 1]();
		while (readSize < len)
		{
			int size = 32 < (len - readSize) ? 32 : len - readSize;
			hr = UvcProperty(UVC_XU_IO_GET, UVCPROPERTY_GET_FLAGS, &readData_[readSize], size, ulBytesReturned);
			readSize += size;
		}
		if (crc16(readData_ + INDEX_FIELD_HEAD_LEN, dataLen) != checksum)
		{
			hr = E_FAIL;
			printf("checksum failed!");
			return hr;
		}
		*data = (char*)(readData_ + INDEX_FIELD_HEAD_LEN);

		//close io
		hr = CloseIO(ulBytesReturned);
		if (FAILED(hr))
		{
			printf("Unable to get property size : %x\n", hr);
		};
		return hr;
	}

	UINT16 DirectShowControl::crc16(UINT8 *data_p, int length)
	{
		UINT8 x;
		UINT16 crc = 0xFFFF;

		while (length--){
			x = crc >> 8 ^ *data_p++;
			x ^= x >> 4;
			crc = (crc << 8) ^ ((UINT16)(x << 12)) ^ ((UINT16)(x << 5)) ^ ((UINT16)x);
		}
		return crc;
	}

	HRESULT DirectShowControl::UvcXuReadVersion()
	{
		if (!pKsControl_)
			return E_FAIL;
		HRESULT hr = S_OK;
		ULONG ulBytesReturned;
		byte start_bytes[8] = { 0x0B };
		hr = UvcProperty(CAMERA_FW_VERSION_IO_START_CS, UVCPROPERTY_SET_FLAGS, start_bytes, sizeof(start_bytes), ulBytesReturned);
		if (hr != S_OK)
		{
			printf("request for read version data failed!\n");
			return -1;
		}

		/*UINT8 read_bytes[8] = { 0 };
		hr = UvcProperty(CAMERA_FW_VERSION_IO_READ_CS, UVCPROPERTY_GET_FLAGS, read_bytes, sizeof(read_bytes), ulBytesReturned);
		if (FAILED(hr))
		{
		return -1;
		}
		INT64 version = ((INT64)read_bytes[7]) | ((INT64)read_bytes[6] << 8ll) | ((INT64)read_bytes[5] << 16ll) | ((INT64)read_bytes[4] << 24ll) |
		((INT64)read_bytes[3] << 32ll) | ((INT64)read_bytes[2] << 40ll) | ((INT64)read_bytes[1] << 48ll) | ((INT64)read_bytes[0] << 56ll);
		printf("fw version: %lld (%02X %02X %02X %02X %02X %02X %02X %02X)\n", version,
		(int)read_bytes[0], (int)read_bytes[1], (int)read_bytes[2], (int)read_bytes[3], (int)read_bytes[4], (int)read_bytes[5], (int)read_bytes[6], (int)read_bytes[7]);
		return version;*/
		UINT8 releasename_buf[32] = { 0 };
		hr = UvcProperty(CAMERA_FW_VERSION_RELEASE_NAME_IO_NAME, UVCPROPERTY_GET_FLAGS, releasename_buf, sizeof(releasename_buf), ulBytesReturned);
		if (FAILED(hr))
		{
			printf("read version(io read release name) failed: %x", hr);
			return E_FAIL;
		}
		releasename_buf[31] = '\0';
		if (releaseName_)
		{
			delete[] releaseName_;
			releaseName_ = nullptr;
		}
		releaseName_ = new char[32]();
		memcpy(releaseName_, releasename_buf, 32);
		return S_OK;
	}

	HRESULT DirectShowControl::ReadBuildDate(char build_date[])
	{
		if (!pKsControl_)
			return E_FAIL;
		HRESULT hr = S_OK;
		ULONG ulBytesReturned;
		byte start_bytes[8] = { 0x0C };
		hr = UvcProperty(CAMERA_FW_VERSION_IO_START_CS, UVCPROPERTY_SET_FLAGS, start_bytes, sizeof(start_bytes), ulBytesReturned);
		if (hr != S_OK)
		{
			printf("request for read build date failed!\n");
			return -1;
		}

		UINT8 buildDate[8] = { 0 };
		hr = UvcProperty(CAMERA_FW_VERSION_IO_READ_CS, UVCPROPERTY_GET_FLAGS, buildDate, sizeof(buildDate), ulBytesReturned);
		if (FAILED(hr))
		{
			printf("read build date failed: %x", hr);
			return E_FAIL;
		}
		memset(build_date, 0, 8);
		memcpy(build_date, &buildDate[1], 7);
		return S_OK;
	}

	char* DirectShowControl::GetReleaseName()
	{
		return releaseName_;
	}

	HRESULT DirectShowControl::StartIO(int index, int len, ULONG &ulBytesReturned)
	{
		HRESULT hr;
		BYTE CmdIn[16] = { 0 };
		CmdIn[0] = 0x31;
		CmdIn[1] = index;
		CmdIn[2] = 0x00;
		CmdIn[3] = 0x00;
		CmdIn[4] = 0x80;
		CmdIn[5] = 0x04;
		CmdIn[6] = len;
		CmdIn[7] = 0;
		CmdIn[8] = 0;
		CmdIn[9] = 0;

		hr = UvcProperty(UVC_XU_IO_SET, UVCPROPERTY_SET_FLAGS, CmdIn, sizeof(CmdIn), ulBytesReturned);
		return hr;
	}

	HRESULT DirectShowControl::CloseIO(ULONG &ulBytesReturned)
	{
		HRESULT hr;
		BYTE CmdIn[16] = { 0 };
		CmdIn[0] = 0x32;
		hr = UvcProperty(UVC_XU_IO_SET, UVCPROPERTY_SET_FLAGS, CmdIn, sizeof(CmdIn), ulBytesReturned);
		return hr;
	}

	HRESULT DirectShowControl::UvcProperty(ULONG propertyId, ULONG flags, LPVOID data, ULONG dataLength, ULONG &ulBytesReturned)
	{
		HRESULT hr = S_OK;
		KSP_NODE  ExtensionProp;
		ExtensionProp.Property.Set = xuGUID_;
		ExtensionProp.Property.Id = propertyId;
		ExtensionProp.Property.Flags = flags;
		ExtensionProp.NodeId = nodeId_;
		ExtensionProp.Reserved = 0;
		hr = pKsControl_->KsProperty((PKSPROPERTY)&ExtensionProp, sizeof(ExtensionProp), data, dataLength, &ulBytesReturned);
		return hr;
	}

	HRESULT DirectShowControl::VideoPropSet(long Property, long lValue, long Flags)
	{
		if (!pAMVideoProcAmp_)
			return E_FAIL;
		HRESULT hr = S_OK;
		hr = pAMVideoProcAmp_->Set(Property, lValue, Flags);
		return hr;
	}

	HRESULT DirectShowControl::VideoPropGet(long Property, long *lValue, long *Flags)
	{
		if (!pAMVideoProcAmp_)
			return E_FAIL;
		HRESULT hr = S_OK;
		hr = pAMVideoProcAmp_->Get(Property, lValue, Flags);
		return hr;
	}

	std::vector<std::string> DirectShowControl::getDevPath()
	{
		return devPath_;
	}


}





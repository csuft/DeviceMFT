#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <assert.h>

#if (defined _WINDOWS)
#include <windows.h>
#include <assert.h>
#include <direct.h>
#include <sys/stat.h> 
#include <io.h>
#else
#include <sys/stat.h> 
#include <unistd.h>
#include <pthread.h>
#endif


#if (defined _WINDOWS)
#define CC_STRCMP(src, dst) _stricmp(src, dst)
#define CC_SPRINTF _snprintf
#else
#define CC_STRCMP(src, dst) strcmp(src, dst)
#define CC_SPRINTF snprintf
#endif

class  CMyLog
{
public:
	CMyLog();
	~CMyLog();

	static CMyLog& GetInstance();
	void Log(unsigned char level, const char* file, int line, const char* fmt, ...);
	void Log(unsigned char level, const char* str);

private:
	void ChangeLogFile();
	// 全局文件指针
	FILE* m_fp;
	unsigned int m_ulFileSize;
	unsigned char m_ucLevel;

	std::string m_path;

#if (defined _WINDOWS)
	HANDLE m_hMutex;
#else
	pthread_mutex_t m_mutex;
#endif

};

#define LOG_LEVEL_ERR  0 
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_DBG  2

#define LOGERR(fmt, ...)  CMyLog::GetInstance().Log(LOG_LEVEL_ERR,  __FILE__, __LINE__, fmt, ##__VA_ARGS__);
#define LOGINFO(fmt, ...) CMyLog::GetInstance().Log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__);
#define LOGDBG(fmt, ...)  CMyLog::GetInstance().Log(LOG_LEVEL_DBG,  __FILE__, __LINE__, fmt, ##__VA_ARGS__);
#endif



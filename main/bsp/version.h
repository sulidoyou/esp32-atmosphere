#ifndef __VERSION_H__
#define __VERSION_H__

// ============================================================
// 固件版本号 - 修改这里即可更新全系统版本
// ============================================================
#define APP_VERSION_MAJOR  1
#define APP_VERSION_MINOR  3
#define APP_VERSION_PATCH  2

// 使用两层宏实现正确的值stringify
#define __STRINGIFY(x) #x
#define _STRINGIFY(x)  __STRINGIFY(x)
#define __JOIN3(a,b,c)  a "." b "." c
#define __JOIN4(a,b,c,d) a "." b "." c "." d

// 版本字符串
// APP_VERSION_STR   = v1.3.1 （用于API和横幅，简短）
// APP_VERSION_FULL  = v1.3.1.0（用于OTA和完整展示）
#define APP_VERSION_STRING  __JOIN3(_STRINGIFY(APP_VERSION_MAJOR), _STRINGIFY(APP_VERSION_MINOR), _STRINGIFY(APP_VERSION_PATCH))
#define APP_VERSION_FULL    __JOIN4(_STRINGIFY(APP_VERSION_MAJOR), _STRINGIFY(APP_VERSION_MINOR), _STRINGIFY(APP_VERSION_PATCH), "0")
#define APP_VERSION_STR     "v" APP_VERSION_STRING
#define APP_VERSION_FULL_STR "v" APP_VERSION_FULL

// 编译时间戳（每次编译自动变化，用于HTML缓存清除）
#define BUILD_DATE_STR __DATE__
#define BUILD_TIME_STR __TIME__

#endif  // __VERSION_H__

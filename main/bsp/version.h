#ifndef __VERSION_H__
#define __VERSION_H__

// ============================================================
// 固件版本号 - 修改这里即可更新全系统版本
// ============================================================
#define APP_VERSION_MAJOR  1
#define APP_VERSION_MINOR  3

#define __XSTRING(x) #x
#define __JOIN3(a,b,c) a "." b "." c
#define __JOIN2(a,b) a "." b

// 版本字符串
// APP_VERSION_STR  = v1.3  （用于API和横幅，简短）
// APP_VERSION_FULL = v1.3.0（用于OTA和完整展示）
#define APP_VERSION_STRING  __JOIN2(__XSTRING(APP_VERSION_MAJOR), __XSTRING(APP_VERSION_MINOR))
#define APP_VERSION_FULL   __JOIN3(__XSTRING(APP_VERSION_MAJOR), __XSTRING(APP_VERSION_MINOR), "0")
#define APP_VERSION_STR    "v" APP_VERSION_STRING
#define APP_VERSION_FULL_STR "v" APP_VERSION_FULL

#endif  // __VERSION_H__

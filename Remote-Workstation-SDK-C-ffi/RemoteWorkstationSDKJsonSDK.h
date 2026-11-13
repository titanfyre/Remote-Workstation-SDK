/* SPDX-License-Identifier: MIT */
#ifndef REMOTE_WORKSTATION_SDK_CSDK_H
#define REMOTE_WORKSTATION_SDK_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT
#endif
#if defined(REMOTE_WORKSTATION_SDK_CSDK_BUILD)
#define REMOTE_WORKSTATION_SDK_CSDK_API __declspec(dllexport)
#else
#define REMOTE_WORKSTATION_SDK_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
REMOTE_WORKSTATION_SDK_CSDK_API uint32_t remote_workstation_sdk_json_open(uint64_t *session) REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT;
REMOTE_WORKSTATION_SDK_CSDK_API uint32_t remote_workstation_sdk_json_close(uint64_t session) REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT;
REMOTE_WORKSTATION_SDK_CSDK_API char *remote_workstation_sdk_json_call(uint64_t session,const char *request,uint32_t length) REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT;
REMOTE_WORKSTATION_SDK_CSDK_API void remote_workstation_sdk_json_free(char *response) REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef REMOTE_WORKSTATION_SDK_CSDK_NOEXCEPT
#endif

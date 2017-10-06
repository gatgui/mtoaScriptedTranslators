#ifndef __common_h__
#define __common_h__

#include "utils/Version.h"
#include "ai.h"

#if AI_VERSION_ARCH_NUM < 5
#  define AiMax MAX
#  define ARRAY() ARRAY
#  define AI_TYPE_VECTOR2 AI_TYPE_POINT2
#  define AI_TYPE_VECTOR2 AI_TYPE_POINT2
#  define AiArraySetVec2 AiArraySetPnt2
#  define BOOL() BOOL
#  define BYTE() BYTE
#  define INT() INT
#  define UINT() UINT
#  define FLT() FLT
#  define VEC2() PNT2
#  define VEC() VEC
#  define RGB() RGB
#  define RGBA() RGBA
#  define pMTX() pMTX
#  define STR() STR
#  define PTR() PTR
#endif // AI_VERSION_ARCH_NUM < 5

#endif

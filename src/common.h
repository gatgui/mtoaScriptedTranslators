#ifndef __common_h__
#define __common_h__

#include "utils/Version.h"
#include "ai.h"

#if MTOA_ARCH_VERSION_NUM == 1 && MTOA_MAJOR_VERSION_NUM <= 3
#  define OLD_API
#endif

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
#  define GET_POINT(out, node, name) AtPoint out = AiNodeGetPnt(node, name);
#  define SET_POINT(node, name, x, y, z) AiNodeSetPnt(node, name, x, y, z);
#else 
#  define GET_POINT(out, node, name) AtVector out = AiNodeGetVec(node, name);
#  define SET_POINT(node, name, x, y, z) AiNodeSetVec(node, name, x, y, z);
#endif // AI_VERSION_ARCH_NUM < 5

#endif

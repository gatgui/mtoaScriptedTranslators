#ifndef __common_h__
#define __common_h__

#include "utils/Version.h"

#if MTOA_ARCH_VERSION_NUM == 1 && MTOA_MAJOR_VERSION_NUM <= 3
#  define OLD_API
#endif

#endif

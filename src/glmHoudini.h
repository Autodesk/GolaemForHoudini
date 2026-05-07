/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

// Disable Houdini SDK warnings
#ifndef HDK_INCLUDES_START
#ifdef _MSC_VER
#define HDK_INCLUDES_START   \
    __pragma(warning(push)); \
    __pragma(warning(disable : 4244 4201 4211 4100 4018 4267 4499 4275 4389 4127 4463 4457 4308 4307 4384 4505 4146 4458 4305));
#else
#define HDK_INCLUDES_START
#endif
#endif

#ifndef HDK_INCLUDES_END
#ifdef _MSC_VER
#define HDK_INCLUDES_END \
    __pragma(warning(pop));
#else
#define HDK_INCLUDES_END
#endif
#endif

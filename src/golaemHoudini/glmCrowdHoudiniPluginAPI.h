/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once
#ifdef _MSC_VER
#define GLM_CROWDHOUDINI_API __declspec(dllexport)
#else
#define GLM_CROWDHOUDINI_API __attribute__((visibility("default")))
#endif

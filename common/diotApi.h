/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:58:47 2009 texane
** Last update Wed Apr 15 22:58:49 2009 texane
*/



#ifndef DIOT_API_H_INCLUDED
# define DIOT_API_H_INCLUDED



#include "diotTypes.h"



/* exported functions
 */

#ifdef DIOT_BUILD_DLL
# define DIOT_API __declspec(dllexport)
#else
# define DIOT_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

DIOT_API enum diotError diotInitialize(void (*)(const struct diotEvent*, void*), void*);
DIOT_API enum diotError diotCleanup(void);
DIOT_API enum diotError diotSetConf(const struct diotConf*);
DIOT_API enum diotError diotGetConf(struct diotConf*);
DIOT_API enum diotError diotSetMmioRanges(const struct diotRange*, unsigned int);
DIOT_API enum diotError diotGetMmioRanges(struct diotRange**, unsigned int*);
DIOT_API enum diotError diotSetIoportRanges(const struct diotRange*, unsigned int);
DIOT_API enum diotError diotGetIoportRanges(struct diotRange**, unsigned int*);
DIOT_API enum diotError diotStartTracing(void);
DIOT_API enum diotError diotStopTracing(void);

#ifdef __cplusplus
}
#endif



#endif /* ! DIOT_API_H_INCLUDED */

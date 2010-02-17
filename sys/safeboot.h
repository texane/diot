/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:58:10 2009 texane
** Last update Wed Apr 15 22:58:11 2009 texane
*/



#ifndef SAFEBOOT_H_INCLUDED
# define SAFEBOOT_H_INCLUDED



#include <wdm.h>



extern PULONG InitSafeBootMode; 


__declspec(inline) static BOOLEAN isSafeBoot(VOID)
{
  if (*InitSafeBootMode > 0)
    return TRUE;
	
  return FALSE;
}



#endif /* ! SAFEBOOT_H_INCLUDED */

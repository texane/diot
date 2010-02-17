/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:23 2009 texane
** Last update Wed Apr 15 22:57:24 2009 texane
*/



#ifndef IA32_H_INCLUDED
# define IA32_H_INCLUDED



#include <wdm.h>



__declspec(naked) static ULONG_PTR ia32ReadCr2(void)
{
  __asm
    {
      mov eax, cr2 ;
      ret ;
    }
}


__declspec(naked) static ULONG_PTR ia32ReadDr6(void)
{
  __asm
    {
      mov eax, dr6 ;
      ret ;
    }
}


ULONG_PTR ia32GetIdtHandler(ULONG);

void ia32MarkPageValid(ULONG_PTR);
void ia32MarkPageInvalid(ULONG_PTR);

void ia32MarkPagesValid(ULONG_PTR, SIZE_T);
void ia32MarkPagesInvalid(ULONG_PTR, SIZE_T);



#endif /* ! IA32_H_INCLUDED */

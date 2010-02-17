/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:12 2009 texane
** Last update Wed Apr 15 22:57:13 2009 texane
*/



#ifndef HOOK_H_INCLUDED
# define HOOK_H_INCLUDED



#include <wdm.h>



typedef PVOID PHOOK_CONTEXT;


PHOOK_CONTEXT HookSetInterruptGate(ULONG_PTR, ULONG_PTR);
VOID HookUnsetInterruptGate(PHOOK_CONTEXT);
VOID HookRefInterruptGate(PHOOK_CONTEXT);
VOID HookUnrefInterruptGate(PHOOK_CONTEXT);

PHOOK_CONTEXT HookSetFunction(ULONG_PTR, ULONG_PTR, SIZE_T);
VOID HookUnsetFunction(PHOOK_CONTEXT);
VOID HookRefFunction(PHOOK_CONTEXT);
VOID HookUnrefFunction(PHOOK_CONTEXT);
ULONG_PTR __fastcall HookCallFunction(ULONG_PTR, PHOOK_CONTEXT);



#endif /* ! HOOK_H_INCLUDED */

/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:57:19 2009 texane
** Last update Wed Apr 15 22:57:20 2009 texane
*/



#include <wdm.h>
#include "ia32.h"



/* ia32 paging
 */

typedef struct _MMPTE_HARDWARE
{
  ULONG Valid : 1;
  ULONG Writable : 1;
  ULONG Owner : 1;
  ULONG WriteThrough : 1;
  ULONG CacheDisable : 1;
  ULONG Accessed : 1;
  ULONG Dirty : 1;
  ULONG LargePage : 1;
  ULONG Global : 1;
  ULONG CopyOnWrite : 1;
  ULONG Prototype : 1;
  ULONG reserved : 1;
  ULONG PageFrameNumber : 20;
} MMPTE_HARDWARE, *PMMPTE_HARDWARE;

typedef struct _MMPTE
{
  union
  {
    ULONG64 Long;
    MMPTE_HARDWARE Hard;
  } u;
} MMPTE, *PMMPTE;

#define PTE_BASE 0xc0000000
#define PDE_BASE 0xc0600000

#define MiGetPteAddress(va) ((PMMPTE)(((((ULONG)(va)) >> 12) << 3) + PTE_BASE))
#define MiGetPdeAddress(va) ((PMMPTE)(((((ULONG)(va)) >> 21) << 3) + PDE_BASE))

#define MI_PDE_MAPS_LARGE_PAGE(_pde) ((_pde)->u.Hard.LargePage == 1)
#define MI_PTE_IS_NOT_EXECUTABLE(_pte) ((_pte)->u.Long & 0x8000000000000000UI64)
#define MI_PTE_IS_EXECUTABLE(_pte) (!MI_PTE_IS_NOT_EXECUTABLE(_pte))
#define MI_PTE_IS_INVALID(_pte) ((_pte)->u.Hard.Valid == 0)


static PMMPTE getPteForVirtual(ULONG_PTR VirtualAddress)
{
  PMMPTE pde;

  pde = MiGetPdeAddress(VirtualAddress);
  if (MI_PDE_MAPS_LARGE_PAGE(pde))
    return pde;

  return MiGetPteAddress(VirtualAddress);
}



/* ia32 idt
 */

typedef struct _IDTR
{
  USHORT Pad;
  USHORT Limite;
  ULONG Base;
} IDTR, *PIDTR;


typedef struct _IDTENTRY
{
  USHORT OffsetLow;
  USHORT Selector;
  UCHAR Reserved;
  UCHAR Always1 : 3;
  UCHAR Type : 1;
  UCHAR Always0 : 1;
  UCHAR Dpl : 2;
  UCHAR Present : 1;
  USHORT OffsetHigh;
} IDTENTRY, *PIDTENTRY;



/* exported
 */


ULONG_PTR ia32GetIdtHandler(ULONG Index)
{
  PIDTENTRY TableEntry;
  ULONG_PTR OldHandler;
  IDTR IDTTable;

  __asm
    {
      sidt IDTTable.Limite ;
    }

  if (Index * sizeof(IDTR) < IDTTable.Limite)
    {
      TableEntry = (PIDTENTRY)IDTTable.Base;
      OldHandler = (ULONG_PTR)((TableEntry[Index].OffsetHigh << 16) | TableEntry[Index].OffsetLow);
    }

  return OldHandler;
}


static void markPage(ULONG_PTR VirtualAddress, BOOLEAN isValid)
{
  PMMPTE const pte = getPteForVirtual(VirtualAddress);

  pte->u.Hard.Valid = isValid;
}


void ia32MarkPageValid(ULONG_PTR VirtualAddress)
{
  markPage(VirtualAddress, TRUE);
}


void ia32MarkPageInvalid(ULONG_PTR VirtualAddress)
{
  markPage(VirtualAddress, 0);
}


static void markPages(ULONG_PTR VirtualAddress, SIZE_T NumberOfBytes, BOOLEAN isValid)
{
#define IA32_PAGE_SIZE 0x1000
#define IA32_PAGE_MASK (IA32_PAGE_SIZE - 1)

  SIZE_T numberOfPages = NumberOfBytes / IA32_PAGE_SIZE;

  if (NumberOfBytes & IA32_PAGE_MASK)
    ++numberOfPages;

  while (numberOfPages--)
    {
      markPage(VirtualAddress, isValid);
      VirtualAddress += IA32_PAGE_SIZE;
    }
}


void ia32MarkPagesValid(ULONG_PTR VirtualAddress, SIZE_T NumberOfBytes)
{
  markPages(VirtualAddress, NumberOfBytes, TRUE);
}


void ia32MarkPagesInvalid(ULONG_PTR VirtualAddress, SIZE_T NumberOfBytes)
{
  markPages(VirtualAddress, NumberOfBytes, FALSE);
}

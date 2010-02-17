/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:56:43 2009 texane
** Last update Mon Apr 20 08:54:58 2009 texane
*/



#ifndef DISASM_H_INCLUDED
# define DISASM_H_INCLUDED



#include <wdm.h>
#include "udis86/udis86.h"
#include "../../common/diotTypes.h"



struct disasmContext
{
  /* wrap since may need syncing */

  struct ud ud;
};



void disasmInitialize(struct disasmContext*);
SIZE_T disasmDecodeIoInsn(struct disasmContext*, ULONG_PTR, const ULONG*, ULONG_PTR*);
SIZE_T disasmDecodeMovInsn(struct disasmContext*, ULONG_PTR, const ULONG*, struct diotInsn*);



#endif /* ! DISASM_H_INCLUDED */

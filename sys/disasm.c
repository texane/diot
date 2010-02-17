/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:56:50 2009 texane
** Last update Mon Apr 20 09:08:31 2009 texane
*/



#include <wdm.h>
#include "disasm.h"
#include "udis86/udis86.h"
#include "../../common/diotTypes.h"
#include "../../common/diotDebug.h"



/* helpers
 */

static unsigned int regIdToDiotReg(enum ud_type RegId)
{
  /* todo: redundant with the code below */

  unsigned int diotReg = DIOT_REG_BAD;

  if (RegId >= UD_R_AL && RegId <= UD_R_BL)
    diotReg = 7 - (RegId - UD_R_AL);
  else if (RegId >= UD_R_AH && RegId <= UD_R_BH)
    diotReg = 7 - (RegId - UD_R_AH);
  else if (RegId >= UD_R_SPL && RegId <= UD_R_DIL)
    diotReg = 7 - (4 + RegId - UD_R_SPL);
  else if (RegId >= UD_R_AX && RegId <= UD_R_DI)
    diotReg = 7 - (RegId - UD_R_AX);
  else if (RegId >= UD_R_EAX && RegId <= UD_R_EDI)
    diotReg = 7 - (RegId - UD_R_EAX);

  return diotReg;
}


static int readRegister(enum ud_type RegId, const ULONG* SavedRegisters, ULONG_PTR* Buffer)
{
  /* pusha register order: eax ecx edx ebx esp ebp esi edi
   */

  unsigned int index;
  unsigned int size;
  unsigned int offset = 0;

  const UCHAR* address;

  if (RegId >= UD_R_AL && RegId <= UD_R_BL)
    {
      size = 1;
      index = RegId - UD_R_AL;
    }
  else if (RegId >= UD_R_AH && RegId <= UD_R_BH)
    {
      size = 1;
      index = RegId - UD_R_AH;
      offset = 1;
    }
  else if (RegId >= UD_R_SPL && RegId <= UD_R_DIL)
    {
      size = 1;
      index = 4 + RegId - UD_R_SPL;
    }
  else if (RegId >= UD_R_AX && RegId <= UD_R_DI)
    {
      size = 2;
      index = RegId - UD_R_AX;
    }
  else if (RegId >= UD_R_EAX && RegId <= UD_R_EDI)
    {
      size = 4;
      index = RegId - UD_R_EAX;
    }
  else
    {
      DIOT_DEBUG_ERROR("unknown RegId: %x\n", RegId);
      return -1;
    }

  address = (const UCHAR*)SavedRegisters + (7 - index) * sizeof(ULONG) + offset;

  *Buffer = 0;

  if (size == 1)
    *(PUCHAR)Buffer = *(const UCHAR*)address;
  else if (size == 2)
    *(PUSHORT)Buffer = *(const USHORT*)address;
  else if (size == 4)
    *(PULONG)Buffer = *(const ULONG*)address;

  return 0;
}


static int getMemoryAddress(const ud_operand_t* Operand, const ULONG* SavedRegisters, ULONG_PTR* Address)
{
  /* base register */

  *Address = 0;

  if (Operand->base >= UD_R_AL && Operand->base <= UD_R_EDI)
    {
      ULONG_PTR base;

      if (readRegister(Operand->base, SavedRegisters, &base) == -1)
	{
	  DIOT_DEBUG_ERROR("readRegister() == -1\n");
	  return -1;
	}

      *Address = base;
    }

  /* index register */

  if (Operand->index >= UD_R_AL && Operand->index <= UD_R_EDI)
    {
      LONG_PTR index;

      if (readRegister(Operand->index, SavedRegisters, (PVOID)&index) == -1)
	{
	  DIOT_DEBUG_ERROR("readRegister() == -1\n");
	  return -1;
	}

      if (Operand->scale)
	*Address += index * Operand->scale;
      else
	*Address += index;
    }

  /* offset */

  if (Operand->offset)
    {
      LONG_PTR offset;

      switch (Operand->offset)
	{
	case 8:
	  offset = Operand->lval.sbyte;
	  break;

	case 16:
	  offset = Operand->lval.sword;
	  break;

	case 32:
	  offset = Operand->lval.sdword;
	  break;

	default:
	  offset = 0;
	  break;
	}

      *Address += offset;
    }

  return 0;
}


static int readMemory(const ud_operand_t* Operand, const ULONG* SavedRegisters, PVOID Buffer)
{
  int res = 0;
  ULONG_PTR address;

  if (getMemoryAddress(Operand, SavedRegisters, &address) == -1)
    {
      DIOT_DEBUG_ERROR("getMemoryAddress() == -1\n");
      return -1;
    }

  switch (Operand->size)
    {
    case 8:
      *(PUCHAR)Buffer = *(const UCHAR*)address;
      break;

    case 16:
      *(PUSHORT)Buffer = *(const USHORT*)address;
      break;

    case 32:
      *(PULONG)Buffer = *(const ULONG*)address;
      break;

    default:
      DIOT_DEBUG_ERROR("Operand->size == %u\n", Operand->size);
      res = -1;
      break;
    }

  return res;
}


static int readImmediate(const ud_operand_t* Operand, const ULONG* SavedRegisters, PVOID Buffer)
{
  int res = 0;

  UNREFERENCED_PARAMETER(SavedRegisters);

  switch (Operand->size)
    {
    case 8:
      *(PUCHAR)Buffer = Operand->lval.ubyte;
      break;

    case 16:
      *(PUSHORT)Buffer = Operand->lval.uword;
      break;

    case 32:
      *(PULONG)Buffer = Operand->lval.udword;
      break;

    default:
      DIOT_DEBUG_ERROR("Operand->size == %u\n", Operand->size);
      res = -1;
      break;
    }

  return res;
}


static int readConstant(const ud_operand_t* Operand, const ULONG* SavedRegisters, PVOID Buffer)
{
  return readImmediate(Operand, SavedRegisters, Buffer);
}



/* exported
 */

void disasmInitialize(struct disasmContext* DisasmContext)
{
  ud_init(&DisasmContext->ud);
  ud_set_mode(&DisasmContext->ud, 32);
  ud_set_syntax(&DisasmContext->ud, UD_SYN_INTEL);
}


SIZE_T disasmDecodeIoInsn(struct disasmContext* DisasmContext,
			  ULONG_PTR Address,
			  const ULONG* SavedRegisters,
			  ULONG_PTR* PortAddress)
{
  struct ud* const ud = &DisasmContext->ud;

  unsigned int insnSize;
  ud_operand_t* operand;

  ud_set_input_buffer(ud, (PVOID)Address, 32);

  insnSize = ud_disassemble(ud);
  if (!insnSize)
    {
      DIOT_DEBUG_ERROR("ud_disassemble(%x)\n", Address);
      goto onError;
    }

  switch (ud->mnemonic)
    {
      /* ins mnemonic
       */

    case UD_Iins:
    case UD_Iinsb:
    case UD_Iinsd:
    case UD_Iinsw:

      operand = &ud->operand[1];

      if (operand->type == UD_NONE)
	{
	  operand->size = 16;
	  operand->type = UD_OP_REG;
	  operand->base = UD_R_DX;
	  operand->index = UD_NONE;
	  operand->offset = UD_NONE;
	  operand->scale = UD_NONE;
	}

      goto inCommonCase;

      /* in mnemonic
       */

    case UD_Iin:
    inCommonCase:

      operand = &ud->operand[1];

      if (operand->type == UD_OP_REG)
	{
	  if (readRegister(operand->base, SavedRegisters, PortAddress) == -1)
	    {
	      DIOT_DEBUG_ERROR("readRegister() == -1\n");
	      goto onError;
	    }
	}
      else if (operand->type == UD_OP_IMM)
	{
	  if (readImmediate(operand, SavedRegisters, PortAddress) == -1)
	    {
	      DIOT_DEBUG_ERROR("readImmediate() == -1\n");
	      goto onError;
	    }
	}
      else
	{
	  DIOT_DEBUG_ERROR("invalid operand %u\n", operand->type);
	  goto onError;
	}

      DbgPrint("in dest <- port[0x%x]\n", *PortAddress);

      break;

      /* outs mnemonic
       */

    case UD_Iouts:
    case UD_Ioutsb:
    case UD_Ioutsd:
    case UD_Ioutsw:

      operand = &ud->operand[0];

      if (operand->type == UD_NONE)
	{
	  operand->size = 16;
	  operand->type = UD_OP_REG;
	  operand->base = UD_R_DX;
	  operand->index = UD_NONE;
	  operand->offset = UD_NONE;
	  operand->scale = UD_NONE;
	}

      goto outCommonCase;

      /* out mnemonic
       */

    case UD_Iout:
    outCommonCase:

      operand = &ud->operand[0];

      if (operand->type == UD_OP_REG)
	{
	  if (readRegister(operand->base, SavedRegisters, PortAddress) == -1)
	    {
	      DIOT_DEBUG_ERROR("readRegister() == -1\n");
	      goto onError;
	    }
	}
      else if (operand->type == UD_OP_IMM)
	{
	  if (readImmediate(operand, SavedRegisters, PortAddress) == -1)
	    {
	      DIOT_DEBUG_ERROR("readImmediate() == -1\n");
	      goto onError;
	    }
	}
      else
	{
	  DIOT_DEBUG_ERROR("invalid operand %u\n", operand->type);
	  goto onError;
	}

      DbgPrint("out port[0x%x], src\n", *PortAddress);

      break;

      /* unknown mnemonic
       */

    default:
      goto onError;
      break;
    }

  /* success */

  return insnSize;

 onError:

  return 0;
}


SIZE_T disasmDecodeMovInsn(struct disasmContext* DisasmContext,
			   ULONG_PTR Address,
			   const ULONG* SavedRegisters,
			   struct diotInsn* Insn)
{
  struct ud* const ud = &DisasmContext->ud;

  unsigned char operandSize = 0;

  ULONG_PTR srcValue;
  ULONG_PTR dstValue;
  unsigned int insnSize;
  enum ud_type regId;
  ud_operand_t* operand;

  Insn->insnType = DIOT_INSN_TYPE_INVALID;
  Insn->insnSize = 0;

  ud_set_input_buffer(ud, (PVOID)Address, 32);

  insnSize = ud_disassemble(ud);
  if (!insnSize)
    {
      DIOT_DEBUG_ERROR("ud_disassemble(%x)\n", Address);
      goto onError;
    }

  if (insnSize > sizeof(Insn->insnBuffer))
    insnSize = sizeof(Insn->insnBuffer);

  memcpy(Insn->insnBuffer, (PVOID)Address, insnSize);

  Insn->insnAddress = Address;
  Insn->insnSize = (UCHAR)insnSize;

  switch (ud->mnemonic)
    {
      /* cmps mnemonic
       */

    case UD_Icmpsb:
      operandSize = 8;
      goto cmpsCommonCase;
      break;

    case UD_Icmpsw:
      operandSize = 16;
      goto cmpsCommonCase;
      break;

    case UD_Icmps:
    case UD_Icmpss:
    case UD_Icmpsd:
      operandSize = 32;
      goto cmpsCommonCase;

    cmpsCommonCase:

      /* implicit operands */

      operand = &ud->operand[0];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_EDI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[1];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_ESI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[2];
      operand->type = UD_NONE;

      goto movCommonCase;

      break;

      /* cmp mnemonic
       */

    case UD_Icmp:
    case UD_Icmppd:
    case UD_Icmpps:

      goto movCommonCase;

      break;

      /* test mnemonic
       */

    case UD_Itest:

      goto movCommonCase ;

      break;

      /* stos mnemonic
       */

    case UD_Istosb:
      operandSize = 8;
      regId = UD_R_AL;
      goto stosCommonCase;

    case UD_Istosw:
      operandSize = 16;
      regId = UD_R_AX;
      goto stosCommonCase;

    case UD_Istos:
    case UD_Istosd:
      operandSize = 32;
      regId = UD_R_EAX;
      goto stosCommonCase ;

    stosCommonCase:

      /* implicit operands */

      operand = &ud->operand[0];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_EDI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[1];
      operand->size = operandSize;
      operand->type = UD_OP_REG;
      operand->base = regId;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[2];
      operand->type = UD_NONE;

      goto movCommonCase ;

      /* lods mnemonic
       */

    case UD_Ilodsb:
      operandSize = 8;
      regId = UD_R_AL;
      goto lodsCommonCase ;

    case UD_Ilodsw:
      operandSize = 16;
      regId = UD_R_AX;
      goto lodsCommonCase ;

    case UD_Ilods:
    case UD_Ilodsd:
      operandSize = 32;
      regId = UD_R_EAX;
      goto lodsCommonCase ;

    lodsCommonCase:

      /* implicit operands */

      operand = &ud->operand[0];
      operand->size = operandSize;
      operand->type = UD_OP_REG;
      operand->base = regId;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[1];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_ESI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[2];
      operand->type = UD_NONE;

      goto movCommonCase ;

      /* movs mnemonic
       */

    case UD_Imovsb:
      operandSize = 8;
      goto movsCommonCase ;

    case UD_Imovsw:
      operandSize = 16;
      goto movsCommonCase ;

    case UD_Imovs:
    case UD_Imovsd:
    case UD_Imovsx:
    case UD_Imovsxd:
      operandSize = 32;
      goto movsCommonCase ;

    movsCommonCase:

      /* implicit operands */

      operand = &ud->operand[0];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_EDI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[1];
      operand->size = operandSize;
      operand->type = UD_OP_MEM;
      operand->base = UD_R_ESI;
      operand->index = UD_NONE;
      operand->offset = UD_NONE;
      operand->scale = UD_NONE;

      operand = &ud->operand[2];
      operand->type = UD_NONE;

      goto movCommonCase ;

      /* mov mnemonic
       */

    case UD_Imov:
    case UD_Imovapd:
    case UD_Imovaps:
    case UD_Imovd:
    case UD_Imovdq2q:
    case UD_Imovdqa:
    case UD_Imovdqu:
    case UD_Imovhlps:
    case UD_Imovhpd:
    case UD_Imovhps:
    case UD_Imovlhps:
    case UD_Imovlpd:
    case UD_Imovlps:
    case UD_Imovmskpd:
    case UD_Imovmskps:
    case UD_Imovnig:
    case UD_Imovntdq:
    case UD_Imovnti:
    case UD_Imovntpd:
    case UD_Imovntps:
    case UD_Imovntq:
    case UD_Imovq:
    case UD_Imovq2dq:
    case UD_Imovqa:
    case UD_Imovzx:
    movCommonCase:

      if (ud->operand[0].type == UD_OP_REG)
	{
	  if (ud->operand[1].type != UD_OP_MEM)
	    {
	      DIOT_DEBUG_ERROR("invalid operand type(%u)\n", ud->operand[1].type);
	      goto onError;
	    }

	  if (getMemoryAddress(&ud->operand[1], SavedRegisters, &srcValue) == -1)
	    {
	      DIOT_DEBUG_ERROR("getMemoryAddress() == -1\n");
	      goto onError;
	    }

	  Insn->insnType = DIOT_INSN_TYPE_MEM_TO_REG;

	  if (operandSize == 0)
	    operandSize = ud->operand[1].size;

	  Insn->operandSize = operandSize;

	  Insn->dstAddress = regIdToDiotReg(ud->operand[0].base);
	  Insn->srcAddress = srcValue;

	  /* value read after */

	  Insn->operandValue = 0;
	}
      else if (ud->operand[0].type == UD_OP_MEM)
	{
	  if (getMemoryAddress(&ud->operand[0], SavedRegisters, &dstValue) == -1)
	    {
	      DIOT_DEBUG_ERROR("getMemoryAddress() == -1\n");
	      goto onError;
	    }

	  if (operandSize == 0)
	    operandSize = ud->operand[0].size;

	  Insn->operandSize = operandSize;
	  Insn->dstAddress = dstValue;

	  switch (ud->operand[1].type)
	    {
	    case UD_OP_REG:
	      {
		if (readRegister(ud->operand[1].base, SavedRegisters, &srcValue) == -1)
		  {
		    DIOT_DEBUG_ERROR("readRegister() == -1\n");
		    goto onError;
		  }

		Insn->insnType = DIOT_INSN_TYPE_REG_TO_MEM;
		Insn->srcAddress = regIdToDiotReg(ud->operand[1].base);
		Insn->operandValue = srcValue;

		break;
	      }

	    case UD_OP_IMM:
	      {
		if (readImmediate(&ud->operand[1], SavedRegisters, &srcValue) == -1)
		  {
		    DIOT_DEBUG_ERROR("readImmediate() == -1\n");
		    goto onError;
		  }

		Insn->insnType = DIOT_INSN_TYPE_IMM_TO_MEM;
		Insn->srcAddress = 0;
		Insn->operandValue = srcValue;

		break;
	      }

	    case UD_OP_CONST:
	      {
		if (readConstant(&ud->operand[1], SavedRegisters, &srcValue) == -1)
		  {
		    DIOT_DEBUG_ERROR("readConstant() == -1\n");
		    goto onError;
		  }

		Insn->insnType = DIOT_INSN_TYPE_IMM_TO_MEM;
		Insn->srcAddress = 0;
		Insn->operandValue = srcValue;

		break;
	      }

	    case UD_OP_MEM:
	      {
		if (getMemoryAddress(&ud->operand[1], SavedRegisters, &srcValue) == -1)
		  {
		    DIOT_DEBUG_ERROR("getMemoryAddress() == -1\n");
		    goto onError;
		  }

		Insn->insnType = DIOT_INSN_TYPE_MEM_TO_MEM;
		Insn->srcAddress = srcValue;
		Insn->operandValue = 0;

		break;
	      }

	    default:
	      {
		DIOT_DEBUG_ERROR("invalid operand type(%u)\n", ud->operand[1].type);
		goto onError;
	      }
	    }
	}
      else
	{
	  DIOT_DEBUG_ERROR("invalid operands(%u, %u)\n", ud->operand[0].type, ud->operand[1].type);
	  goto onError;
	}

      break;

      /* unknown mnemonic
       */

    default:
      DIOT_DEBUG_ERROR("invalid mnemnoic: 0x%x\n", ud->mnemonic);
      break;
    }

  /* success */

  return (SIZE_T)insnSize;

 onError:

  return 0;
}


#if 0 /* units */


static ULONG baz = 0x2a2a2a2a;
static PULONG bar = &baz;


__declspec(naked) static void insnLines(void)
{
  __asm
    {
      in eax, dx ;

      rep lods ;
      rep stosb ;
      rep movsw ;
      rep movsd ;

      mov dword ptr [eax], 0x2a ;
      mov dword ptr [eax], ebx ;

      mov dword ptr [bar], 0x2a ;
      mov dword ptr [bar], ebx ;

      mov eax, baz ;
      mov ebx, dword ptr [bar] ;
    }
}


static void DisasmUnit(void)
{
  static const SIZE_T insnCount = 10;

  const UCHAR* currentInsn = (PVOID)insnLines;
  SIZE_T insnSize;
  SIZE_T i;

  DbgPrint("{@baz == 0x%08x}\n", bar);

  for (i = 0; i < insnCount; ++i)
    {
      DbgPrint("[%x]: ", i);

      __asm
	{
	  pushad ;

	  pushad ;
	  mov dword ptr [esp + 0x1c], 0x7000 ;
	  mov dword ptr [esp + 0x18], 0x6000 ;
	  mov dword ptr [esp + 0x14], 0x5000 ;
	  mov dword ptr [esp + 0x10], 0x4000 ;
	  mov dword ptr [esp + 0x0c], 0x3000 ;
	  mov dword ptr [esp + 0x08], 0x2000 ;
	  mov dword ptr [esp + 0x04], 0x1000 ;
	  mov dword ptr [esp + 0x00], 0x0000 ;

	  push esp ;
	  push currentInsn ;
	  ;	  call DisasmDecodeMovInsn ; stdcall ;
	  call DisasmDecodeIoInsn ; stdcall ;
	  mov insnSize, eax ;

	  popad ; 

	  popad ;
	}

      if (!insnSize)
	break;
      
      currentInsn += insnSize;
    }
}


#endif /* units */

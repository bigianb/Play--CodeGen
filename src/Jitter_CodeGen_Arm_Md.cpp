#include "Jitter_CodeGen_Arm.h"

using namespace Jitter;

void CCodeGen_Arm::LoadMemory128AddressInRegister(CArmAssembler::REGISTER dstReg, CSymbol* symbol)
{
	switch(symbol->m_type)
	{
	case SYM_RELATIVE128:
		LoadRelative128AddressInRegister(dstReg, symbol);
		break;
	case SYM_TEMPORARY128:
		LoadTemporary128AddressInRegister(dstReg, symbol);
		break;
	default:
		assert(0);
		break;
	}
}

void CCodeGen_Arm::LoadRelative128AddressInRegister(CArmAssembler::REGISTER dstReg, CSymbol* symbol)
{
	assert(symbol->m_type == SYM_RELATIVE128);

	uint8 immediate = 0;
	uint8 shiftAmount = 0;
	if(!TryGetAluImmediateParams(symbol->m_valueLow, immediate, shiftAmount))
	{
		throw std::runtime_error("Failed to build immediate for symbol.");
	}
	m_assembler.Add(dstReg, g_baseRegister, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
}

void CCodeGen_Arm::LoadTemporary128AddressInRegister(CArmAssembler::REGISTER dstReg, CSymbol* symbol)
{
	assert(symbol->m_type == SYM_TEMPORARY128);

	uint8 immediate = 0;
	uint8 shiftAmount = 0;
	if(!TryGetAluImmediateParams(symbol->m_stackLocation + m_stackLevel, immediate, shiftAmount))
	{
		throw std::runtime_error("Failed to build immediate for symbol.");
	}
	m_assembler.Add(dstReg, CArmAssembler::rSP, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
}

void CCodeGen_Arm::LoadTemporary256ElementAddressInRegister(CArmAssembler::REGISTER dstReg, CSymbol* symbol, uint32 offset)
{
	assert(symbol->m_type == SYM_TEMPORARY256);

	uint8 immediate = 0;
	uint8 shiftAmount = 0;
	if(!TryGetAluImmediateParams(symbol->m_stackLocation + m_stackLevel + offset, immediate, shiftAmount))
	{
		throw std::runtime_error("Failed to build immediate for symbol.");
	}
	m_assembler.Add(dstReg, CArmAssembler::rSP, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
}

template <typename MDOP>
void CCodeGen_Arm::Emit_Md_MemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto dstReg = CArmAssembler::q0;
	auto src1Reg = CArmAssembler::q1;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src1AddrReg, src1);

	m_assembler.Vld1_32x4(src1Reg, src1AddrReg);
	((m_assembler).*(MDOP::OpReg()))(dstReg, src1Reg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

template <typename MDOP>
void CCodeGen_Arm::Emit_Md_MemMemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto src2AddrReg = CArmAssembler::r2;
	auto dstReg = CArmAssembler::q0;
	auto src1Reg = CArmAssembler::q1;
	auto src2Reg = CArmAssembler::q2;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src1AddrReg, src1);
	LoadMemory128AddressInRegister(src2AddrReg, src2);

	m_assembler.Vld1_32x4(src1Reg, src1AddrReg);
	m_assembler.Vld1_32x4(src2Reg, src2AddrReg);
	((m_assembler).*(MDOP::OpReg()))(dstReg, src1Reg, src2Reg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Mov_MemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto tmpReg = CArmAssembler::q0;
	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src1AddrReg, src1);
	m_assembler.Vld1_32x4(tmpReg, src1AddrReg);
	m_assembler.Vst1_32x4(tmpReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Not_MemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto zeroReg = CArmAssembler::q0;
	auto tmpReg = CArmAssembler::q1;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src1AddrReg, src1);

	m_assembler.Vld1_32x4(tmpReg, src1AddrReg);
	m_assembler.Veor(zeroReg, zeroReg, zeroReg);
	m_assembler.Vorn(tmpReg, zeroReg, tmpReg);
	m_assembler.Vst1_32x4(tmpReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Srl256_MemMemCst(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(src1->m_type == SYM_TEMPORARY256);
	assert(src2->m_type == SYM_CONSTANT);

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto dstReg = CArmAssembler::q0;

	uint32 offset = (src2->m_valueLow & 0x7F) / 8;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadTemporary256ElementAddressInRegister(src1AddrReg, src1, offset);

	m_assembler.Vld1_32x4(dstReg, src1AddrReg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Srl256_MemMemVar(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(src1->m_type == SYM_TEMPORARY256);

	auto offsetRegister = CArmAssembler::r0;
	auto dstAddrReg = CArmAssembler::r1;
	auto src1AddrReg = CArmAssembler::r2;
	auto src2Register = PrepareSymbolRegisterUse(src2, CArmAssembler::r3);

	auto dstReg = CArmAssembler::q0;

	auto offsetShift = CArmAssembler::MakeConstantShift(CArmAssembler::SHIFT_LSR, 3);

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadTemporary256ElementAddressInRegister(src1AddrReg, src1, 0);

	//Compute offset and modify address
	m_assembler.And(offsetRegister, src2Register, CArmAssembler::MakeImmediateAluOperand(0x7F, 0));
	m_assembler.Mov(offsetRegister, CArmAssembler::MakeRegisterAluOperand(offsetRegister, offsetShift));
	m_assembler.Add(src1AddrReg, src1AddrReg, offsetRegister);

	m_assembler.Vld1_32x4(dstReg, src1AddrReg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_LoadFromRef_MemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto src1AddrReg = CArmAssembler::r0;
	auto dstAddrReg = CArmAssembler::r1;

	auto dstReg = CArmAssembler::q0;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemoryReferenceInRegister(src1AddrReg, src1);

	m_assembler.Vld1_32x4(dstReg, src1AddrReg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_StoreAtRef_MemMem(const STATEMENT& statement)
{
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	auto src1AddrReg = CArmAssembler::r0;
	auto src2AddrReg = CArmAssembler::r1;

	auto src2Reg = CArmAssembler::q0;

	LoadMemoryReferenceInRegister(src1AddrReg, src1);
	LoadMemory128AddressInRegister(src2AddrReg, src2);

	m_assembler.Vld1_32x4(src2Reg, src2AddrReg);
	m_assembler.Vst1_32x4(src2Reg, src1AddrReg);
}

void CCodeGen_Arm::Emit_Md_MovMasked_MemMemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(dst->Equals(src1));

	auto mask = static_cast<uint8>(statement.jmpCondition);

	auto dstAddrReg = CArmAssembler::r0;
	auto src2AddrReg = CArmAssembler::r2;
	auto tmpReg = CArmAssembler::r3;
	auto dstReg = CArmAssembler::q0;
	auto src2Reg = CArmAssembler::q2;
	auto dstRegLo = static_cast<CArmAssembler::DOUBLE_REGISTER>(dstReg + 0);
	auto dstRegHi = static_cast<CArmAssembler::DOUBLE_REGISTER>(dstReg + 1);
	auto src2RegLo = static_cast<CArmAssembler::DOUBLE_REGISTER>(src2Reg + 0);
	auto src2RegHi = static_cast<CArmAssembler::DOUBLE_REGISTER>(src2Reg + 1);

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src2AddrReg, src2);

	m_assembler.Vld1_32x4(dstReg, dstAddrReg);
	m_assembler.Vld1_32x4(src2Reg, src2AddrReg);
	for(unsigned int i = 0; i < 4; i++)
	{
		if(mask & (1 << i))
		{
			m_assembler.Vmov(tmpReg, (i & 2) ? src2RegHi : src2RegLo, (i & 1));
			m_assembler.Vmov((i & 2) ? dstRegHi : dstRegLo, tmpReg, (i & 1));
		}
	}

	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Expand_MemReg(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto tmpReg = CArmAssembler::q0;

	LoadMemory128AddressInRegister(dstAddrReg, dst);

	m_assembler.Vdup(tmpReg, g_registers[src1->m_valueLow]);
	m_assembler.Vst1_32x4(tmpReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Expand_MemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1Reg = CArmAssembler::r1;
	auto tmpReg = CArmAssembler::q0;

	LoadMemoryInRegister(src1Reg, src1);
	LoadMemory128AddressInRegister(dstAddrReg, dst);

	m_assembler.Vdup(tmpReg, src1Reg);
	m_assembler.Vst1_32x4(tmpReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_Expand_MemCst(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1Reg = CArmAssembler::r1;
	auto tmpReg = CArmAssembler::q0;

	LoadConstantInRegister(src1Reg, src1->m_valueLow);
	LoadMemory128AddressInRegister(dstAddrReg, dst);

	m_assembler.Vdup(tmpReg, src1Reg);
	m_assembler.Vst1_32x4(tmpReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_Md_PackHB_MemMemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	auto dstAddrReg = CArmAssembler::r0;
	auto src1AddrReg = CArmAssembler::r1;
	auto src2AddrReg = CArmAssembler::r2;
	auto dstReg = CArmAssembler::q0;
	auto src1Reg = CArmAssembler::q1;
	auto src2Reg = CArmAssembler::q2;

	LoadMemory128AddressInRegister(dstAddrReg, dst);
	LoadMemory128AddressInRegister(src1AddrReg, src1);
	LoadMemory128AddressInRegister(src2AddrReg, src2);

	m_assembler.Vld1_32x4(src1Reg, src1AddrReg);
	m_assembler.Vld1_32x4(src2Reg, src2AddrReg);
	m_assembler.Vmovn_I16(static_cast<CArmAssembler::DOUBLE_REGISTER>(dstReg + 1), src1Reg);
	m_assembler.Vmovn_I16(static_cast<CArmAssembler::DOUBLE_REGISTER>(dstReg + 0), src2Reg);
	m_assembler.Vst1_32x4(dstReg, dstAddrReg);
}

void CCodeGen_Arm::Emit_MergeTo256_MemMemMem(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(dst->m_type == SYM_TEMPORARY256);

	auto dstLoAddrReg = CArmAssembler::r0;
	auto dstHiAddrReg = CArmAssembler::r1;
	auto src1AddrReg = CArmAssembler::r2;
	auto src2AddrReg = CArmAssembler::r3;
	auto src1Reg = CArmAssembler::q0;
	auto src2Reg = CArmAssembler::q1;

	LoadTemporary256ElementAddressInRegister(dstLoAddrReg, dst, 0x00);
	LoadTemporary256ElementAddressInRegister(dstHiAddrReg, dst, 0x10);
	LoadMemory128AddressInRegister(src1AddrReg, src1);
	LoadMemory128AddressInRegister(src2AddrReg, src2);

	m_assembler.Vld1_32x4(src1Reg, src1AddrReg);
	m_assembler.Vld1_32x4(src2Reg, src2AddrReg);
	m_assembler.Vst1_32x4(src1Reg, dstLoAddrReg);
	m_assembler.Vst1_32x4(src2Reg, dstHiAddrReg);
}

CCodeGen_Arm::CONSTMATCHER CCodeGen_Arm::g_mdConstMatchers[] = 
{
	{ OP_MD_ADD_W,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_ADDW>					},

	{ OP_MD_SUB_B,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_SUBB>					},
	{ OP_MD_SUB_W,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_SUBW>					},

	{ OP_MD_ADDUS_B,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_ADDBUS>				},
	{ OP_MD_ADDUS_W,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_ADDWUS>				},

	{ OP_MD_CMPEQ_W,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_CMPEQW>				},

	{ OP_MD_CMPGT_H,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_CMPGTH>				},

	{ OP_MD_MIN_H,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_MINH>					},
	{ OP_MD_MIN_W,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_MINW>					},

	{ OP_MD_MAX_H,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_MAXH>					},
	{ OP_MD_MAX_W,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_MAXW>					},

	{ OP_MD_ADD_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_ADDS>					},
	{ OP_MD_SUB_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_SUBS>					},
	{ OP_MD_MUL_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_MULS>					},

	{ OP_MD_ABS_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_NIL,				&CCodeGen_Arm::Emit_Md_MemMem<MDOP_ABSS>					},
	{ OP_MD_MIN_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<FPUMDOP_MIN>				},
	{ OP_MD_MAX_S,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<FPUMDOP_MAX>				},

	{ OP_MD_AND,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_AND>					},
	{ OP_MD_OR,					MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_OR>					},
	{ OP_MD_XOR,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MemMemMem<MDOP_XOR>					},

	{ OP_MD_NOT,				MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_NIL,				&CCodeGen_Arm::Emit_Md_Not_MemMem							},

	{ OP_MD_SRL256,				MATCH_VARIABLE128,			MATCH_MEMORY256,			MATCH_VARIABLE,			&CCodeGen_Arm::Emit_Md_Srl256_MemMemVar						},
	{ OP_MD_SRL256,				MATCH_VARIABLE128,			MATCH_MEMORY256,			MATCH_CONSTANT,			&CCodeGen_Arm::Emit_Md_Srl256_MemMemCst						},

	{ OP_MD_TOSINGLE,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_NIL,				&CCodeGen_Arm::Emit_Md_MemMem<MDOP_TOSINGLE>				},
	{ OP_MD_TOWORD_TRUNCATE,	MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_NIL,				&CCodeGen_Arm::Emit_Md_MemMem<MDOP_TOWORD>					},

	{ OP_MOV,					MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_NIL,				&CCodeGen_Arm::Emit_Md_Mov_MemMem							},

	{ OP_LOADFROMREF,			MATCH_MEMORY128,			MATCH_MEM_REF,				MATCH_NIL,				&CCodeGen_Arm::Emit_Md_LoadFromRef_MemMem					},
	{ OP_STOREATREF,			MATCH_NIL,					MATCH_MEM_REF,				MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_StoreAtRef_MemMem					},

	{ OP_MD_MOV_MASKED,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_MovMasked_MemMemMem					},

	{ OP_MD_EXPAND,				MATCH_MEMORY128,			MATCH_REGISTER,				MATCH_NIL,				&CCodeGen_Arm::Emit_Md_Expand_MemReg						},
	{ OP_MD_EXPAND,				MATCH_MEMORY128,			MATCH_MEMORY,				MATCH_NIL,				&CCodeGen_Arm::Emit_Md_Expand_MemMem						},
	{ OP_MD_EXPAND,				MATCH_MEMORY128,			MATCH_CONSTANT,				MATCH_NIL,				&CCodeGen_Arm::Emit_Md_Expand_MemCst						},

	{ OP_MD_PACK_HB,			MATCH_MEMORY128,			MATCH_MEMORY128,			MATCH_MEMORY128,		&CCodeGen_Arm::Emit_Md_PackHB_MemMemMem						},

	{ OP_MERGETO256,			MATCH_MEMORY256,			MATCH_VARIABLE128,			MATCH_VARIABLE128,		&CCodeGen_Arm::Emit_MergeTo256_MemMemMem					},

	{ OP_MOV,					MATCH_NIL,					MATCH_NIL,					MATCH_NIL,				NULL														},
};

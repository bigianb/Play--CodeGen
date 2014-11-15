#include "Jitter_CodeGen_Arm.h"
#include "ObjectFile.h"

using namespace Jitter;

CArmAssembler::REGISTER CCodeGen_Arm::g_baseRegister = CArmAssembler::r11;

CArmAssembler::REGISTER CCodeGen_Arm::g_registers[MAX_REGISTERS] =
{
	CArmAssembler::r4,
	CArmAssembler::r5,
	CArmAssembler::r6,
	CArmAssembler::r7,
	CArmAssembler::r8,
	CArmAssembler::r10,
};

CArmAssembler::REGISTER CCodeGen_Arm::g_paramRegs[MAX_PARAMS] =
{
	CArmAssembler::r0,
	CArmAssembler::r1,
	CArmAssembler::r2,
	CArmAssembler::r3,
};

extern "C" uint32 CodeGen_Arm_div_unsigned(uint32 a, uint32 b)
{
	return a / b;
}

extern "C" int32 CodeGen_Arm_div_signed(int32 a, int32 b)
{
	return a / b;
}

extern "C" uint32 CodeGen_Arm_mod_unsigned(uint32 a, uint32 b)
{
	return a % b;
}

extern "C" int32 CodeGen_Arm_mod_signed(int32 a, int32 b)
{
	return a % b;
}

template <typename ALUOP>
void CCodeGen_Arm::Emit_Alu_GenericAnyAny(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	auto dstReg = PrepareSymbolRegister(dst, CArmAssembler::r0);
	auto src1Reg = PrepareSymbolRegister(src1, CArmAssembler::r1);
	auto src2Reg = PrepareSymbolRegister(src2, CArmAssembler::r2);
	((m_assembler).*(ALUOP::OpReg()))(dstReg, src1Reg, src2Reg);
	CommitSymbolRegister(dst, dstReg);
}

template <typename ALUOP>
void CCodeGen_Arm::Emit_Alu_GenericAnyCst(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(src2->m_type == SYM_CONSTANT);

	auto dstReg = PrepareSymbolRegister(dst, CArmAssembler::r0);
	auto src1Reg = PrepareSymbolRegister(src1, CArmAssembler::r1);
	uint32 cst = src2->m_valueLow;

	bool supportsNegative	= ALUOP::OpImmNeg() != NULL;
	bool supportsComplement = ALUOP::OpImmNot() != NULL;
	
	uint8 immediate = 0;
	uint8 shiftAmount = 0;
	if(TryGetAluImmediateParams(cst, immediate, shiftAmount))
	{
		((m_assembler).*(ALUOP::OpImm()))(dstReg, src1Reg, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
	}
	else if(supportsNegative && TryGetAluImmediateParams(-static_cast<int32>(cst), immediate, shiftAmount))
	{
		((m_assembler).*(ALUOP::OpImmNeg()))(dstReg, src1Reg, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
	}
	else if(supportsComplement && TryGetAluImmediateParams(~cst, immediate, shiftAmount))
	{
		((m_assembler).*(ALUOP::OpImmNot()))(dstReg, src1Reg, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
	}
	else
	{
		auto cstReg = PrepareSymbolRegister(src2, CArmAssembler::r2);
		assert(cstReg != dstReg && cstReg != src1Reg);
		((m_assembler).*(ALUOP::OpReg()))(dstReg, src1Reg, cstReg);
	}

	CommitSymbolRegister(dst, dstReg);
}

#define ALU_CONST_MATCHERS(ALUOP_CST, ALUOP) \
	{ ALUOP_CST,	MATCH_ANY,		MATCH_ANY,		MATCH_CONSTANT,	&CCodeGen_Arm::Emit_Alu_GenericAnyCst<ALUOP>		}, \
	{ ALUOP_CST,	MATCH_ANY,		MATCH_ANY,		MATCH_ANY,		&CCodeGen_Arm::Emit_Alu_GenericAnyAny<ALUOP>		},

#include "Jitter_CodeGen_Arm_Div.h"
#include "Jitter_CodeGen_Arm_Mul.h"

template <CArmAssembler::SHIFT shiftType>
void CCodeGen_Arm::Emit_Shift_Generic(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();

	auto dstReg = PrepareSymbolRegister(dst, CArmAssembler::r0);
	auto src1Reg = PrepareSymbolRegister(src1, CArmAssembler::r1);
	auto shift = GetAluShiftFromSymbol(shiftType, src2, CArmAssembler::r2);
	m_assembler.Mov(dstReg, CArmAssembler::MakeRegisterAluOperand(src1Reg, shift));
	CommitSymbolRegister(dst, dstReg);
}

CCodeGen_Arm::CONSTMATCHER CCodeGen_Arm::g_constMatchers[] = 
{ 
	{ OP_LABEL,			MATCH_NIL,			MATCH_NIL,			MATCH_NIL,			&CCodeGen_Arm::MarkLabel									},

	{ OP_NOP,			MATCH_NIL,			MATCH_NIL,			MATCH_NIL,			&CCodeGen_Arm::Emit_Nop										},
	
	{ OP_MOV,			MATCH_REGISTER,		MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RegReg								},
	{ OP_MOV,			MATCH_REGISTER,		MATCH_RELATIVE,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RegRel								},
	{ OP_MOV,			MATCH_REGISTER,		MATCH_CONSTANT,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RegCst								},
	{ OP_MOV,			MATCH_RELATIVE,		MATCH_RELATIVE,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RelRel								},
	{ OP_MOV,			MATCH_RELATIVE,		MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RelReg								},
	{ OP_MOV,			MATCH_RELATIVE,		MATCH_CONSTANT,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RelCst								},
	{ OP_MOV,			MATCH_RELATIVE,		MATCH_TEMPORARY,	MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_RelTmp								},
	{ OP_MOV,			MATCH_TEMPORARY,	MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Mov_TmpReg								},

	ALU_CONST_MATCHERS(OP_ADD, ALUOP_ADD)
	ALU_CONST_MATCHERS(OP_SUB, ALUOP_SUB)
	ALU_CONST_MATCHERS(OP_AND, ALUOP_AND)
	ALU_CONST_MATCHERS(OP_OR,  ALUOP_OR)
	ALU_CONST_MATCHERS(OP_XOR, ALUOP_XOR)
	
	{ OP_SRL,			MATCH_ANY,			MATCH_ANY,			MATCH_ANY,			&CCodeGen_Arm::Emit_Shift_Generic<CArmAssembler::SHIFT_LSR>	},
	{ OP_SRA,			MATCH_ANY,			MATCH_ANY,			MATCH_ANY,			&CCodeGen_Arm::Emit_Shift_Generic<CArmAssembler::SHIFT_ASR>	},
	{ OP_SLL,			MATCH_ANY,			MATCH_ANY,			MATCH_ANY,			&CCodeGen_Arm::Emit_Shift_Generic<CArmAssembler::SHIFT_LSL>	},

	{ OP_PARAM,			MATCH_NIL,			MATCH_CONTEXT,		MATCH_NIL,			&CCodeGen_Arm::Emit_Param_Ctx								},
	{ OP_PARAM,			MATCH_NIL,			MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Param_Reg								},
	{ OP_PARAM,			MATCH_NIL,			MATCH_RELATIVE,		MATCH_NIL,			&CCodeGen_Arm::Emit_Param_Rel								},
	{ OP_PARAM,			MATCH_NIL,			MATCH_CONSTANT,		MATCH_NIL,			&CCodeGen_Arm::Emit_Param_Cst								},
	{ OP_PARAM,			MATCH_NIL,			MATCH_TEMPORARY,	MATCH_NIL,			&CCodeGen_Arm::Emit_Param_Tmp								},

	{ OP_CALL,			MATCH_NIL,			MATCH_CONSTANT,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_Call									},
	
	{ OP_RETVAL,		MATCH_REGISTER,		MATCH_NIL,			MATCH_NIL,			&CCodeGen_Arm::Emit_RetVal_Reg								},
	{ OP_RETVAL,		MATCH_TEMPORARY,	MATCH_NIL,			MATCH_NIL,			&CCodeGen_Arm::Emit_RetVal_Tmp								},

	{ OP_JMP,			MATCH_NIL,			MATCH_NIL,			MATCH_NIL,			&CCodeGen_Arm::Emit_Jmp										},

	{ OP_CONDJMP,		MATCH_NIL,			MATCH_REGISTER,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_CondJmp_RegReg							},
	{ OP_CONDJMP,		MATCH_NIL,			MATCH_REGISTER,		MATCH_MEMORY,		&CCodeGen_Arm::Emit_CondJmp_RegMem							},
	{ OP_CONDJMP,		MATCH_NIL,			MATCH_REGISTER,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_CondJmp_RegCst							},
	{ OP_CONDJMP,		MATCH_NIL,			MATCH_MEMORY,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_CondJmp_MemCst							},
	
	{ OP_CMP,			MATCH_ANY,			MATCH_ANY,			MATCH_CONSTANT,		&CCodeGen_Arm::Emit_Cmp_AnyAnyCst							},
	{ OP_CMP,			MATCH_ANY,			MATCH_ANY,			MATCH_ANY,			&CCodeGen_Arm::Emit_Cmp_AnyAnyAny							},

	{ OP_EXTLOW64,		MATCH_REGISTER,		MATCH_TEMPORARY64,	MATCH_NIL,			&CCodeGen_Arm::Emit_ExtLow64RegTmp64						},
	{ OP_EXTLOW64,		MATCH_RELATIVE,		MATCH_TEMPORARY64,	MATCH_NIL,			&CCodeGen_Arm::Emit_ExtLow64RelTmp64						},

	{ OP_EXTHIGH64,		MATCH_REGISTER,		MATCH_TEMPORARY64,	MATCH_NIL,			&CCodeGen_Arm::Emit_ExtHigh64RegTmp64						},
	{ OP_EXTHIGH64,		MATCH_RELATIVE,		MATCH_TEMPORARY64,	MATCH_NIL,			&CCodeGen_Arm::Emit_ExtHigh64RelTmp64						},

	{ OP_NOT,			MATCH_REGISTER,		MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Not_RegReg								},
	{ OP_NOT,			MATCH_RELATIVE,		MATCH_REGISTER,		MATCH_NIL,			&CCodeGen_Arm::Emit_Not_RelReg								},
	
	{ OP_DIV,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_DivTmp64RegReg<false>					},
	{ OP_DIV,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_DivTmp64RegCst<false>					},
//	{ OP_DIV,			MATCH_TEMPORARY64,	MATCH_RELATIVE,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_DivTmp64RelReg<false>					},

	{ OP_DIVS,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_DivTmp64RegReg<true>					},
	{ OP_DIVS,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_DivTmp64RegCst<true>					},
//	{ OP_DIVS,			MATCH_TEMPORARY64,	MATCH_RELATIVE,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_DivTmp64RelReg<true>					},
	
	{ OP_MUL,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_MulTmp64RegReg<false>					},
	{ OP_MUL,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_MulTmp64RegCst<false>					},
	{ OP_MUL,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_RELATIVE,		&CCodeGen_Arm::Emit_MulTmp64RegRel<false>					},
	{ OP_MUL,			MATCH_TEMPORARY64,	MATCH_RELATIVE,		MATCH_RELATIVE,		&CCodeGen_Arm::Emit_MulTmp64RelRel<false>					},

	{ OP_MULS,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_MulTmp64RegReg<true>					},
	{ OP_MULS,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_MulTmp64RegCst<true>					},
	{ OP_MULS,			MATCH_TEMPORARY64,	MATCH_REGISTER,		MATCH_RELATIVE,		&CCodeGen_Arm::Emit_MulTmp64RegRel<true>					},
	{ OP_MULS,			MATCH_TEMPORARY64,	MATCH_RELATIVE,		MATCH_RELATIVE,		&CCodeGen_Arm::Emit_MulTmp64RelRel<true>					},

	{ OP_ADDREF,		MATCH_TMP_REF,		MATCH_REL_REF,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_AddRef_TmpRelReg						},
	{ OP_ADDREF,		MATCH_TMP_REF,		MATCH_REL_REF,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_AddRef_TmpRelCst						},
	
	{ OP_LOADFROMREF,	MATCH_REGISTER,		MATCH_TMP_REF,		MATCH_NIL,			&CCodeGen_Arm::Emit_LoadFromRef_RegTmp						},

	{ OP_STOREATREF,	MATCH_NIL,			MATCH_TMP_REF,		MATCH_REGISTER,		&CCodeGen_Arm::Emit_StoreAtRef_TmpReg						},
	{ OP_STOREATREF,	MATCH_NIL,			MATCH_TMP_REF,		MATCH_RELATIVE,		&CCodeGen_Arm::Emit_StoreAtRef_TmpRel						},
	{ OP_STOREATREF,	MATCH_NIL,			MATCH_TMP_REF,		MATCH_CONSTANT,		&CCodeGen_Arm::Emit_StoreAtRef_TmpCst						},
	
	{ OP_MOV,			MATCH_NIL,			MATCH_NIL,			MATCH_NIL,			NULL														},
};

CCodeGen_Arm::CCodeGen_Arm()
: m_stream(nullptr)
, m_literalPool(nullptr)
, m_literalPoolReloc(nullptr)
{
	for(CONSTMATCHER* constMatcher = g_constMatchers; constMatcher->emitter != NULL; constMatcher++)
	{
		MATCHER matcher;
		matcher.op			= constMatcher->op;
		matcher.dstType		= constMatcher->dstType;
		matcher.src1Type	= constMatcher->src1Type;
		matcher.src2Type	= constMatcher->src2Type;
		matcher.emitter		= std::bind(constMatcher->emitter, this, std::placeholders::_1);
		m_matchers.insert(MatcherMapType::value_type(matcher.op, matcher));
	}

	for(CONSTMATCHER* constMatcher = g_fpuConstMatchers; constMatcher->emitter != NULL; constMatcher++)
	{
		MATCHER matcher;
		matcher.op			= constMatcher->op;
		matcher.dstType		= constMatcher->dstType;
		matcher.src1Type	= constMatcher->src1Type;
		matcher.src2Type	= constMatcher->src2Type;
		matcher.emitter		= std::bind(constMatcher->emitter, this, std::placeholders::_1);
		m_matchers.insert(MatcherMapType::value_type(matcher.op, matcher));
	}

	m_literalPool = new uint32[LITERAL_POOL_SIZE];
	m_literalPoolReloc = new bool[LITERAL_POOL_SIZE];
}

CCodeGen_Arm::~CCodeGen_Arm()
{
	delete [] m_literalPool;
	delete [] m_literalPoolReloc;
}

unsigned int CCodeGen_Arm::GetAvailableRegisterCount() const
{
	return MAX_REGISTERS;
}

unsigned int CCodeGen_Arm::GetAvailableMdRegisterCount() const
{
	return 0;
}

unsigned int CCodeGen_Arm::GetAddressSize() const
{
	return 4;
}

bool CCodeGen_Arm::CanHold128BitsReturnValueInRegisters() const
{
	return false;
}

void CCodeGen_Arm::SetStream(Framework::CStream* stream)
{
	m_stream = stream;
	m_assembler.SetStream(stream);
}

void CCodeGen_Arm::RegisterExternalSymbols(CObjectFile* objectFile) const
{
	objectFile->AddExternalSymbol("_CodeGen_Arm_div_unsigned",	reinterpret_cast<void*>(&CodeGen_Arm_div_unsigned));
	objectFile->AddExternalSymbol("_CodeGen_Arm_div_signed",	reinterpret_cast<void*>(&CodeGen_Arm_div_signed));
	objectFile->AddExternalSymbol("_CodeGen_Arm_mod_unsigned",	reinterpret_cast<void*>(&CodeGen_Arm_mod_unsigned));
	objectFile->AddExternalSymbol("_CodeGen_Arm_mod_signed",	reinterpret_cast<void*>(&CodeGen_Arm_mod_signed));
}

void CCodeGen_Arm::GenerateCode(const StatementList& statements, unsigned int stackSize)
{
	uint16 registerSave = GetSavedRegisterList(GetRegisterUsage(statements));
	m_lastLiteralPtr = 0;

	Emit_Prolog(stackSize, registerSave);

	for(const auto& statement : statements)
	{
		bool found = false;
		auto begin = m_matchers.lower_bound(statement.op);
		auto end = m_matchers.upper_bound(statement.op);

		for(auto matchIterator(begin); matchIterator != end; matchIterator++)
		{
			const MATCHER& matcher(matchIterator->second);
			if(!SymbolMatches(matcher.dstType, statement.dst)) continue;
			if(!SymbolMatches(matcher.src1Type, statement.src1)) continue;
			if(!SymbolMatches(matcher.src2Type, statement.src2)) continue;
			matcher.emitter(statement);
			found = true;
			break;
		}
		assert(found);
		if(!found)
		{
			throw std::runtime_error("No suitable emitter found for statement.");
		}
	}

	Emit_Epilog(stackSize, registerSave);

	m_assembler.ResolveLabelReferences();
	m_assembler.ClearLabels();
	m_labels.clear();
	
	DumpLiteralPool();
}

uint16 CCodeGen_Arm::GetSavedRegisterList(uint32 registerUsage)
{
	uint16 registerSave = 0;
	for(unsigned int i = 0; i < MAX_REGISTERS; i++)
	{
		if((1 << i) & registerUsage)
		{
			registerSave |= (1 << g_registers[i]);
		}
	}
	registerSave |= (1 << CArmAssembler::r11);
	registerSave |= (1 << CArmAssembler::rLR);
	return registerSave;
}

void CCodeGen_Arm::Emit_Prolog(unsigned int stackSize, uint16 registerSave)
{
	m_assembler.Stmdb(CArmAssembler::rSP, registerSave);
	if(stackSize != 0)
	{
		m_assembler.Sub(CArmAssembler::rSP, CArmAssembler::rSP, CArmAssembler::MakeImmediateAluOperand(stackSize, 0));
	}
	m_assembler.Mov(CArmAssembler::r11, CArmAssembler::r0);
	m_stackLevel = 0;
}

void CCodeGen_Arm::Emit_Epilog(unsigned int stackSize, uint16 registerSave)
{
	if(stackSize != 0)
	{
		m_assembler.Add(CArmAssembler::rSP, CArmAssembler::rSP, CArmAssembler::MakeImmediateAluOperand(stackSize, 0));
	}
	m_assembler.Ldmia(CArmAssembler::rSP, registerSave);
	m_assembler.Bx(CArmAssembler::rLR);
}

uint32 CCodeGen_Arm::RotateRight(uint32 value)
{
	uint32 carry = value & 1;
	value >>= 1;
	value |= carry << 31;
	return value;
}

uint32 CCodeGen_Arm::RotateLeft(uint32 value)
{
	uint32 carry = value >> 31;
	value <<= 1;
	value |= carry;
	return value;
}

bool CCodeGen_Arm::TryGetAluImmediateParams(uint32 constant, uint8& immediate, uint8& shiftAmount)
{
	uint32 shadowConstant = constant;
	shiftAmount = 0xFF;
	
	for(unsigned int i = 0; i < 16; i++)
	{
		if((shadowConstant & 0xFF) == shadowConstant)
		{
			shiftAmount = i;
			break;
		}
		shadowConstant = RotateLeft(shadowConstant);
		shadowConstant = RotateLeft(shadowConstant);
	}
	
	if(shiftAmount != 0xFF)
	{
		immediate = static_cast<uint8>(shadowConstant);
		return true;
	}
	else
	{
		return false;
	}
}

void CCodeGen_Arm::LoadConstantInRegister(CArmAssembler::REGISTER registerId, uint32 constant, bool relocatable)
{	
	if(!relocatable)
	{
		//Try normal move
		{
			uint8 immediate = 0;
			uint8 shiftAmount = 0;
			if(TryGetAluImmediateParams(constant, immediate, shiftAmount))
			{
				m_assembler.Mov(registerId, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
				return;
			}
		}
	
		//Try not move
		{
			uint8 immediate = 0;
			uint8 shiftAmount = 0;
			if(TryGetAluImmediateParams(~constant, immediate, shiftAmount))
			{
				m_assembler.Mvn(registerId, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
				return;
			}
		}
	}
		
	//Store as constant in literal table
	{
		//Search for an existing literal
		unsigned int literalPtr = -1;
		for(unsigned int i = 0; i < m_lastLiteralPtr; i++)
		{
			if((m_literalPool[i] == constant) && (m_literalPoolReloc[i] == relocatable)) 
			{
				literalPtr = i;
				break;
			}
		}

		if(literalPtr == -1)
		{
			assert(m_lastLiteralPtr != LITERAL_POOL_SIZE);
			literalPtr = m_lastLiteralPtr++;
			m_literalPool[literalPtr] = constant;
			m_literalPoolReloc[literalPtr] = relocatable;
		}
		
		LITERAL_POOL_REF reference;
		reference.poolPtr		= literalPtr;
		reference.dstRegister	= registerId;
		reference.offset		= static_cast<unsigned int>(m_stream->Tell());
		m_literalPoolRefs.push_back(reference);
		
		//Write a blank instruction
		m_stream->Write32(0);
	}
}

void CCodeGen_Arm::LoadMemoryInRegister(CArmAssembler::REGISTER registerId, CSymbol* src)
{
	switch(src->m_type)
	{
	case SYM_RELATIVE:
		LoadRelativeInRegister(registerId, src);
		break;
	case SYM_TEMPORARY:
		LoadTemporaryInRegister(registerId, src);
		break;
	default:
		assert(0);
		break;
	}
}

void CCodeGen_Arm::StoreRegisterInMemory(CSymbol* dst, CArmAssembler::REGISTER registerId)
{
	switch(dst->m_type)
	{
	case SYM_RELATIVE:
		StoreRegisterInRelative(dst, registerId);
		break;
	case SYM_TEMPORARY:
		StoreRegisterInTemporary(dst, registerId);
		break;
	default:
		assert(0);
		break;
	}
}

void CCodeGen_Arm::LoadRelativeInRegister(CArmAssembler::REGISTER registerId, CSymbol* src)
{
	assert(src->m_type == SYM_RELATIVE);
	assert((src->m_valueLow & 0x03) == 0x00);
	m_assembler.Ldr(registerId, g_baseRegister, CArmAssembler::MakeImmediateLdrAddress(src->m_valueLow));
}

void CCodeGen_Arm::StoreRegisterInRelative(CSymbol* dst, CArmAssembler::REGISTER registerId)
{
	assert(dst->m_type == SYM_RELATIVE);
	assert((dst->m_valueLow & 0x03) == 0x00);
	m_assembler.Str(registerId, g_baseRegister, CArmAssembler::MakeImmediateLdrAddress(dst->m_valueLow));
}

void CCodeGen_Arm::LoadTemporaryInRegister(CArmAssembler::REGISTER registerId, CSymbol* src)
{
	assert(src->m_type == SYM_TEMPORARY);
	m_assembler.Ldr(registerId, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src->m_stackLocation + m_stackLevel));
}

void CCodeGen_Arm::StoreRegisterInTemporary(CSymbol* dst, CArmAssembler::REGISTER registerId)
{
	assert(dst->m_type == SYM_TEMPORARY);
	m_assembler.Str(registerId, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(dst->m_stackLocation + m_stackLevel));
}

void CCodeGen_Arm::LoadRelativeReferenceInRegister(CArmAssembler::REGISTER registerId, CSymbol* src)
{
	assert(src->m_type == SYM_REL_REFERENCE);
	assert((src->m_valueLow & 0x03) == 0x00);
	m_assembler.Ldr(registerId, g_baseRegister, CArmAssembler::MakeImmediateLdrAddress(src->m_valueLow));
}

void CCodeGen_Arm::LoadTemporaryReferenceInRegister(CArmAssembler::REGISTER registerId, CSymbol* src)
{
	assert(src->m_type == SYM_TMP_REFERENCE);
	m_assembler.Ldr(registerId, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src->m_stackLocation + m_stackLevel));
}

void CCodeGen_Arm::StoreInRegisterTemporaryReference(CSymbol* dst, CArmAssembler::REGISTER registerId)
{
	assert(dst->m_type == SYM_TMP_REFERENCE);
	m_assembler.Str(registerId, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(dst->m_stackLocation + m_stackLevel));
}

CArmAssembler::AluLdrShift CCodeGen_Arm::GetAluShiftFromSymbol(CArmAssembler::SHIFT shiftType, CSymbol* symbol, CArmAssembler::REGISTER preferedRegister)
{
	switch(symbol->m_type)
	{
	case SYM_REGISTER:
		return CArmAssembler::MakeVariableShift(shiftType, g_registers[symbol->m_valueLow]);
		break;
	case SYM_TEMPORARY:
	case SYM_RELATIVE:
		LoadMemoryInRegister(preferedRegister, symbol);
		return CArmAssembler::MakeVariableShift(shiftType, preferedRegister);
		break;
	case SYM_CONSTANT:
		return CArmAssembler::MakeConstantShift(shiftType, static_cast<uint8>(symbol->m_valueLow));
		break;
	default:
		throw std::runtime_error("Invalid symbol type.");
		break;
	}
}

CArmAssembler::REGISTER CCodeGen_Arm::PrepareSymbolRegister(CSymbol* symbol, CArmAssembler::REGISTER preferedRegister)
{
	switch(symbol->m_type)
	{
	case SYM_REGISTER:
		return g_registers[symbol->m_valueLow];
		break;
	case SYM_TEMPORARY:
	case SYM_RELATIVE:
		LoadMemoryInRegister(preferedRegister, symbol);
		return preferedRegister;
		break;
	case SYM_CONSTANT:
		LoadConstantInRegister(preferedRegister, symbol->m_valueLow);
		return preferedRegister;
		break;
	default:
		throw std::runtime_error("Invalid symbol type.");
		break;
	}
}

void CCodeGen_Arm::CommitSymbolRegister(CSymbol* symbol, CArmAssembler::REGISTER usedRegister)
{
	switch(symbol->m_type)
	{
	case SYM_REGISTER:
		assert(usedRegister == g_registers[symbol->m_valueLow]);
		break;
	case SYM_TEMPORARY:
	case SYM_RELATIVE:
		StoreRegisterInMemory(symbol, usedRegister);
		break;
	default:
		throw std::runtime_error("Invalid symbol type.");
		break;
	}
}

void CCodeGen_Arm::DumpLiteralPool()
{
	if(m_lastLiteralPtr)
	{
		uint32 literalPoolPos = static_cast<uint32>(m_stream->Tell());
		m_stream->Write(m_literalPool, sizeof(uint32) * m_lastLiteralPtr);
		
		for(unsigned int i = 0; i < m_lastLiteralPtr; i++)
		{
			if(m_literalPoolReloc[i])
			{
				if(m_externalSymbolReferencedHandler)
				{
					m_externalSymbolReferencedHandler(reinterpret_cast<void*>(m_literalPool[i]), literalPoolPos + (i * 4));
				}
			}
		}

		for(const auto& reference : m_literalPoolRefs)
		{
			m_stream->Seek(reference.offset, Framework::STREAM_SEEK_SET);
			uint32 literalOffset = (reference.poolPtr * 4 + literalPoolPos) - reference.offset - 8;
			m_assembler.Ldr(reference.dstRegister, CArmAssembler::rPC, CArmAssembler::MakeImmediateLdrAddress(literalOffset));
		}
	}
	
	m_literalPoolRefs.clear();
	m_lastLiteralPtr = 0;
}

CArmAssembler::LABEL CCodeGen_Arm::GetLabel(uint32 blockId)
{
	CArmAssembler::LABEL result;
	LabelMapType::const_iterator labelIterator(m_labels.find(blockId));
	if(labelIterator == m_labels.end())
	{
		result = m_assembler.CreateLabel();
		m_labels[blockId] = result;
	}
	else
	{
		result = labelIterator->second;
	}
	return result;
}

void CCodeGen_Arm::MarkLabel(const STATEMENT& statement)
{
	CArmAssembler::LABEL label = GetLabel(statement.jmpBlock);
	m_assembler.MarkLabel(label);
}

void CCodeGen_Arm::Emit_Nop(const STATEMENT& statement)
{
	
}

void CCodeGen_Arm::Emit_Param_Ctx(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(src1->m_type == SYM_CONTEXT);
	assert(m_params.size() < MAX_PARAMS);
	
	void (CArmAssembler::*MovFunction)(CArmAssembler::REGISTER, CArmAssembler::REGISTER) = &CArmAssembler::Mov;
	m_params.push_back(std::bind(MovFunction, &m_assembler, std::placeholders::_1, g_baseRegister));	
}

void CCodeGen_Arm::Emit_Param_Reg(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(src1->m_type == SYM_REGISTER);
	assert(m_params.size() < MAX_PARAMS);
	
	void (CArmAssembler::*MovFunction)(CArmAssembler::REGISTER, CArmAssembler::REGISTER) = &CArmAssembler::Mov;
	m_params.push_back(std::bind(MovFunction, &m_assembler, std::placeholders::_1, g_registers[src1->m_valueLow]));	
}

void CCodeGen_Arm::Emit_Param_Rel(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(src1->m_type == SYM_RELATIVE);
	assert(m_params.size() < MAX_PARAMS);
	
	m_params.push_back(std::bind(&CArmAssembler::Ldr, &m_assembler, std::placeholders::_1, g_baseRegister, CArmAssembler::MakeImmediateLdrAddress(src1->m_valueLow)));	
}


void CCodeGen_Arm::Emit_Param_Cst(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(src1->m_type == SYM_CONSTANT);
	assert(m_params.size() < MAX_PARAMS);
	
	m_params.push_back(std::bind(&CCodeGen_Arm::LoadConstantInRegister, this, std::placeholders::_1, src1->m_valueLow, false));
}

void CCodeGen_Arm::Emit_Param_Tmp(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(src1->m_type == SYM_TEMPORARY);
	assert(m_params.size() < MAX_PARAMS);
	
	m_params.push_back(std::bind(&CArmAssembler::Ldr, &m_assembler, std::placeholders::_1, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src1->m_stackLocation + m_stackLevel)));	
}

void CCodeGen_Arm::Emit_Call(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_CONSTANT);
	assert(src2->m_type == SYM_CONSTANT);

	unsigned int paramCount = src2->m_valueLow;
	
	for(unsigned int i = 0; i < paramCount; i++)
	{
		ParamEmitterFunction emitter(m_params.back());
		m_params.pop_back();
		emitter(g_paramRegs[i]);
	}

	m_assembler.Mov(CArmAssembler::rLR, CArmAssembler::rPC);
	LoadConstantInRegister(CArmAssembler::rPC, src1->m_valueLow, true);
}

void CCodeGen_Arm::Emit_RetVal_Reg(const STATEMENT& statement)
{	
	CSymbol* dst = statement.dst->GetSymbol().get();
	
	assert(dst->m_type == SYM_REGISTER);
	
	m_assembler.Mov(g_registers[dst->m_valueLow], CArmAssembler::r0);
}

void CCodeGen_Arm::Emit_RetVal_Tmp(const STATEMENT& statement)
{	
	CSymbol* dst = statement.dst->GetSymbol().get();
	
	assert(dst->m_type == SYM_TEMPORARY);
	
	StoreRegisterInTemporary(dst, CArmAssembler::r0);
}

void CCodeGen_Arm::Emit_Mov_RegReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	m_assembler.Mov(g_registers[dst->m_valueLow], g_registers[src1->m_valueLow]);
}

void CCodeGen_Arm::Emit_Mov_RegRel(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	m_assembler.Ldr(g_registers[dst->m_valueLow], g_baseRegister, CArmAssembler::MakeImmediateLdrAddress(src1->m_valueLow));
}

void CCodeGen_Arm::Emit_Mov_RegCst(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(dst->m_type  == SYM_REGISTER);
	assert(src1->m_type == SYM_CONSTANT);

	LoadConstantInRegister(g_registers[dst->m_valueLow], src1->m_valueLow);
}

void CCodeGen_Arm::Emit_Mov_RelReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_REGISTER);

	StoreRegisterInRelative(dst, g_registers[src1->m_valueLow]);
}

void CCodeGen_Arm::Emit_Mov_RelRel(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_RELATIVE);
	
	LoadRelativeInRegister(CArmAssembler::r0, src1);
	StoreRegisterInRelative(dst, CArmAssembler::r0);
}

void CCodeGen_Arm::Emit_Mov_RelCst(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_CONSTANT);
	
	LoadConstantInRegister(CArmAssembler::r0, src1->m_valueLow);
	StoreRegisterInRelative(dst, CArmAssembler::r0);
}

void CCodeGen_Arm::Emit_Mov_RelTmp(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_TEMPORARY);
	
	LoadTemporaryInRegister(CArmAssembler::r0, src1);
	StoreRegisterInRelative(dst, CArmAssembler::r0);
}

void CCodeGen_Arm::Emit_Mov_TmpReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_TEMPORARY);
	assert(src1->m_type == SYM_REGISTER);
	
	StoreRegisterInTemporary(dst, g_registers[src1->m_valueLow]);
}

void CCodeGen_Arm::Emit_ExtLow64RegTmp64(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(dst->m_type  == SYM_REGISTER);
	assert(src1->m_type == SYM_TEMPORARY64);

	m_assembler.Ldr(g_registers[dst->m_valueLow], CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src1->m_stackLocation + m_stackLevel + 0));
}

void CCodeGen_Arm::Emit_ExtLow64RelTmp64(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_TEMPORARY64);
	
	CArmAssembler::REGISTER dstReg = CArmAssembler::r0;
	m_assembler.Ldr(dstReg, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src1->m_stackLocation + m_stackLevel + 0));
	StoreRegisterInRelative(dst, dstReg);
}

void CCodeGen_Arm::Emit_ExtHigh64RegTmp64(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(dst->m_type  == SYM_REGISTER);
	assert(src1->m_type == SYM_TEMPORARY64);

	m_assembler.Ldr(g_registers[dst->m_valueLow], CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src1->m_stackLocation + m_stackLevel + 4));
}

void CCodeGen_Arm::Emit_ExtHigh64RelTmp64(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_TEMPORARY64);
	
	CArmAssembler::REGISTER dstReg = CArmAssembler::r0;
	m_assembler.Ldr(dstReg, CArmAssembler::rSP, CArmAssembler::MakeImmediateLdrAddress(src1->m_stackLocation + m_stackLevel + 4));
	StoreRegisterInRelative(dst, dstReg);
}

void CCodeGen_Arm::Emit_Jmp(const STATEMENT& statement)
{
	m_assembler.BCc(CArmAssembler::CONDITION_AL, GetLabel(statement.jmpBlock));
}

void CCodeGen_Arm::Emit_CondJmp(const STATEMENT& statement)
{
	CArmAssembler::LABEL label(GetLabel(statement.jmpBlock));
	
	switch(statement.jmpCondition)
	{
		case CONDITION_EQ:
			m_assembler.BCc(CArmAssembler::CONDITION_EQ, label);
			break;
		case CONDITION_NE:
			m_assembler.BCc(CArmAssembler::CONDITION_NE, label);
			break;
		case CONDITION_LE:
			m_assembler.BCc(CArmAssembler::CONDITION_LE, label);
			break;
		case CONDITION_GT:
			m_assembler.BCc(CArmAssembler::CONDITION_GT, label);
			break;
		default:
			assert(0);
			break;
	}
}

void CCodeGen_Arm::Emit_CondJmp_RegReg(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_REGISTER);
	assert(src2->m_type == SYM_REGISTER);
	
	m_assembler.Cmp(g_registers[src1->m_valueLow], g_registers[src2->m_valueLow]);
	
	Emit_CondJmp(statement);
}

void CCodeGen_Arm::Emit_CondJmp_RegMem(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_REGISTER);
	
	CArmAssembler::REGISTER tmpReg = CArmAssembler::r1;
	LoadMemoryInRegister(tmpReg, src2);
	
	m_assembler.Cmp(g_registers[src1->m_valueLow], tmpReg);
	
	Emit_CondJmp(statement);
}

void CCodeGen_Arm::Emit_CondJmp_RegCst(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();	
	
	assert(src1->m_type == SYM_REGISTER);
	assert(src2->m_type == SYM_CONSTANT);
	
	Cmp_GenericRegCst(g_registers[src1->m_valueLow], src2->m_valueLow);
	Emit_CondJmp(statement);
}

void CCodeGen_Arm::Emit_CondJmp_MemCst(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src2->m_type == SYM_CONSTANT);
	
	CArmAssembler::REGISTER tmpReg = CArmAssembler::r1;
	LoadMemoryInRegister(tmpReg, src1);
	Cmp_GenericRegCst(tmpReg, src2->m_valueLow);
	Emit_CondJmp(statement);
}

void CCodeGen_Arm::Cmp_GetFlag(CArmAssembler::REGISTER registerId, Jitter::CONDITION condition)
{
	CArmAssembler::ImmediateAluOperand falseOperand(CArmAssembler::MakeImmediateAluOperand(0, 0));
	CArmAssembler::ImmediateAluOperand trueOperand(CArmAssembler::MakeImmediateAluOperand(1, 0));
	switch(condition)
	{
		case CONDITION_NE:
			m_assembler.MovCc(CArmAssembler::CONDITION_EQ, registerId, falseOperand);
			m_assembler.MovCc(CArmAssembler::CONDITION_NE, registerId, trueOperand);
			break;
		case CONDITION_LT:
			m_assembler.MovCc(CArmAssembler::CONDITION_GE, registerId, falseOperand);
			m_assembler.MovCc(CArmAssembler::CONDITION_LT, registerId, trueOperand);
			break;
		case CONDITION_GT:
			m_assembler.MovCc(CArmAssembler::CONDITION_LE, registerId, falseOperand);
			m_assembler.MovCc(CArmAssembler::CONDITION_GT, registerId, trueOperand);
			break;
		case CONDITION_BL:
			m_assembler.MovCc(CArmAssembler::CONDITION_CS, registerId, falseOperand);
			m_assembler.MovCc(CArmAssembler::CONDITION_CC, registerId, trueOperand);
			break;
		case CONDITION_AB:
			m_assembler.MovCc(CArmAssembler::CONDITION_LS, registerId, falseOperand);
			m_assembler.MovCc(CArmAssembler::CONDITION_HI, registerId, trueOperand);
			break;
		default:
			assert(0);
			break;
	}
}

void CCodeGen_Arm::Cmp_GenericRegCst(CArmAssembler::REGISTER src1, uint32 src2)
{
	uint8 immediate = 0;
	uint8 shiftAmount = 0;
	if(TryGetAluImmediateParams(src2, immediate, shiftAmount))
	{
		m_assembler.Cmp(src1, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
	}
	else if(TryGetAluImmediateParams(-static_cast<int32>(src2), immediate, shiftAmount))
	{
		m_assembler.Cmn(src1, CArmAssembler::MakeImmediateAluOperand(immediate, shiftAmount));
	}
	else
	{
		CArmAssembler::REGISTER cstReg = CArmAssembler::r2;
		assert(cstReg != src1);
		LoadConstantInRegister(cstReg, src2);
		m_assembler.Cmp(src1, cstReg);
	}
}

void CCodeGen_Arm::Emit_Cmp_AnyAnyAny(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	auto dstReg = PrepareSymbolRegister(dst, CArmAssembler::r0);
	auto src1Reg = PrepareSymbolRegister(src1, CArmAssembler::r1);
	auto src2Reg = PrepareSymbolRegister(src2, CArmAssembler::r2);

	m_assembler.Cmp(src1Reg, src2Reg);
	Cmp_GetFlag(dstReg, statement.jmpCondition);
	CommitSymbolRegister(dst, dstReg);
}

void CCodeGen_Arm::Emit_Cmp_AnyAnyCst(const STATEMENT& statement)
{
	auto dst = statement.dst->GetSymbol().get();
	auto src1 = statement.src1->GetSymbol().get();
	auto src2 = statement.src2->GetSymbol().get();

	assert(src2->m_type == SYM_CONSTANT);

	auto dstReg = PrepareSymbolRegister(dst, CArmAssembler::r0);
	auto src1Reg = PrepareSymbolRegister(src1, CArmAssembler::r1);
	auto cst = src2->m_valueLow;

	Cmp_GenericRegCst(src1Reg, cst);
	Cmp_GetFlag(dstReg, statement.jmpCondition);
	CommitSymbolRegister(dst, dstReg);
}

void CCodeGen_Arm::Emit_Not_RegReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();

	assert(dst->m_type  == SYM_REGISTER);
	assert(src1->m_type == SYM_REGISTER);

	m_assembler.Mvn(g_registers[dst->m_valueLow], g_registers[src1->m_valueLow]);
}

void CCodeGen_Arm::Emit_Not_RelReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_RELATIVE);
	assert(src1->m_type == SYM_REGISTER);
	
	CArmAssembler::REGISTER tmpReg = CArmAssembler::r1;
	m_assembler.Mvn(tmpReg, g_registers[src1->m_valueLow]);
	StoreRegisterInRelative(dst, tmpReg);
}

void CCodeGen_Arm::Emit_AddRef_TmpRelReg(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(dst->m_type  == SYM_TMP_REFERENCE);
	assert(src1->m_type == SYM_REL_REFERENCE);
	assert(src2->m_type == SYM_REGISTER);
	
	CArmAssembler::REGISTER tmpReg = CArmAssembler::r0;
	
	LoadRelativeReferenceInRegister(tmpReg, src1);
	m_assembler.Add(tmpReg, tmpReg, g_registers[src2->m_valueLow]);
	StoreInRegisterTemporaryReference(dst, tmpReg);
}

void CCodeGen_Arm::Emit_AddRef_TmpRelCst(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(dst->m_type  == SYM_TMP_REFERENCE);
	assert(src1->m_type == SYM_REL_REFERENCE);
	assert(src2->m_type == SYM_CONSTANT);
	
	CArmAssembler::REGISTER tmpReg0 = CArmAssembler::r0;
	CArmAssembler::REGISTER tmpReg1 = CArmAssembler::r1;
	
	LoadRelativeReferenceInRegister(tmpReg0, src1);
	LoadConstantInRegister(tmpReg1, src2->m_valueLow);
	m_assembler.Add(tmpReg0, tmpReg0, tmpReg1);
	StoreInRegisterTemporaryReference(dst, tmpReg0);
}

void CCodeGen_Arm::Emit_LoadFromRef_RegTmp(const STATEMENT& statement)
{
	CSymbol* dst = statement.dst->GetSymbol().get();
	CSymbol* src1 = statement.src1->GetSymbol().get();
	
	assert(dst->m_type  == SYM_REGISTER);
	assert(src1->m_type == SYM_TMP_REFERENCE);
	
	CArmAssembler::REGISTER addressReg = CArmAssembler::r0;
	LoadTemporaryReferenceInRegister(addressReg, src1);
	m_assembler.Ldr(g_registers[dst->m_valueLow], addressReg, CArmAssembler::MakeImmediateLdrAddress(0));
}

void CCodeGen_Arm::Emit_StoreAtRef_TmpReg(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_TMP_REFERENCE);
	assert(src2->m_type == SYM_REGISTER);
	
	CArmAssembler::REGISTER addressReg = CArmAssembler::r0;
	
	LoadTemporaryReferenceInRegister(addressReg, src1);
	m_assembler.Str(g_registers[src2->m_valueLow], addressReg, CArmAssembler::MakeImmediateLdrAddress(0));
}

void CCodeGen_Arm::Emit_StoreAtRef_TmpRel(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_TMP_REFERENCE);
	assert(src2->m_type == SYM_RELATIVE);
	
	CArmAssembler::REGISTER addressReg = CArmAssembler::r0;
	CArmAssembler::REGISTER valueReg = CArmAssembler::r1;
	
	LoadTemporaryReferenceInRegister(addressReg, src1);
	LoadRelativeInRegister(valueReg, src2);
	m_assembler.Str(valueReg, addressReg, CArmAssembler::MakeImmediateLdrAddress(0));
}

void CCodeGen_Arm::Emit_StoreAtRef_TmpCst(const STATEMENT& statement)
{
	CSymbol* src1 = statement.src1->GetSymbol().get();
	CSymbol* src2 = statement.src2->GetSymbol().get();
	
	assert(src1->m_type == SYM_TMP_REFERENCE);
	assert(src2->m_type == SYM_CONSTANT);
	
	CArmAssembler::REGISTER addressReg = CArmAssembler::r0;
	CArmAssembler::REGISTER valueReg = CArmAssembler::r1;
	
	LoadTemporaryReferenceInRegister(addressReg, src1);
	LoadConstantInRegister(valueReg, src2->m_valueLow);
	m_assembler.Str(valueReg, addressReg, CArmAssembler::MakeImmediateLdrAddress(0));
}

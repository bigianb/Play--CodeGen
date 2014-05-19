#ifndef _JITTER_SYMBOL_H_
#define _JITTER_SYMBOL_H_

#include "Types.h"
#include <memory>
#include <string>
#include <assert.h>

namespace Jitter
{
	enum SYM_TYPE
	{
		SYM_CONTEXT,

		SYM_CONSTANT,
		SYM_RELATIVE,
		SYM_TEMPORARY,
		SYM_REGISTER,

		SYM_REL_REFERENCE,
		SYM_TMP_REFERENCE,

		SYM_RELATIVE64,
		SYM_TEMPORARY64,
		SYM_CONSTANT64,

		SYM_RELATIVE128,
		SYM_TEMPORARY128,

		SYM_TEMPORARY256,

		SYM_FP_REL_SINGLE,
		SYM_FP_TMP_SINGLE,

		SYM_FP_REL_INT32,
	};

	class CSymbol
	{
	public:
		CSymbol(SYM_TYPE type, uint32 valueLow, uint32 valueHigh)
			: m_type(type)
			, m_valueLow(valueLow)
			, m_valueHigh(valueHigh)
		{
			m_rangeBegin = -1;
			m_rangeEnd = -1;
			m_firstUse = -1;
			m_firstDef = -1;
			m_lastDef = -1;
			m_stackLocation = -1;
			m_useCount = 0;
			m_aliased = false;

			m_regAlloc_register = -1;
			m_regAlloc_loadBeforeIdx = -1;
			m_regAlloc_saveAfterIdx = -1;
			m_regAlloc_notAllocatedAfterIdx = -1;
		}

		virtual ~CSymbol()
		{
			
		}

		std::string ToString() const
		{
			switch(m_type)
			{
			case SYM_CONTEXT:
				return "CTX";
				break;
			case SYM_CONSTANT:
				return std::to_string(m_valueLow);
				break;
			case SYM_RELATIVE:
				return "REL[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_TEMPORARY:
				return "TMP[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_REL_REFERENCE:
				return "REL&[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_TMP_REFERENCE:
				return "TMP&[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_CONSTANT64:
				return "CST64[" + std::to_string(m_valueLow) + ", " + std::to_string(m_valueHigh) + "]";
				break;
			case SYM_TEMPORARY64:
				return "TMP64[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_RELATIVE64:
				return "REL64[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_REGISTER:
				return "REG[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_FP_REL_SINGLE:
				return "REL(FP_S)[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_FP_REL_INT32:
				return "REL(FP_I32)[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_FP_TMP_SINGLE:
				return "TMP(FP_S)[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_RELATIVE128:
				return "REL128[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_TEMPORARY128:
				return "TMP128[" + std::to_string(m_valueLow) + "]";
				break;
			case SYM_TEMPORARY256:
				return "TMP256[" + std::to_string(m_valueLow) + "]";
				break;
			default:
				return "";
				break;
			}
		}

		int GetSize() const
		{
			switch(m_type)
			{
			case SYM_RELATIVE:
			case SYM_REGISTER:
			case SYM_CONSTANT:
			case SYM_TEMPORARY:
				return 4;
				break;
			case SYM_RELATIVE64:
			case SYM_TEMPORARY64:
			case SYM_CONSTANT64:
				return 8;
				break;
			case SYM_RELATIVE128:
			case SYM_TEMPORARY128:
				return 16;
				break;
			case SYM_TEMPORARY256:
				return 32;
				break;
			case SYM_FP_REL_SINGLE:
			case SYM_FP_TMP_SINGLE:
			case SYM_FP_REL_INT32:
				return 4;
				break;
			case SYM_REL_REFERENCE:
			case SYM_TMP_REFERENCE:
				return sizeof(void*);
				break;
			default:
				assert(0);
				return 4;
				break;
			}
		}

		bool IsRelative() const
		{
			return 
				(m_type == SYM_RELATIVE) || 
				(m_type == SYM_RELATIVE64) || 
				(m_type == SYM_RELATIVE128) || 
				(m_type == SYM_REL_REFERENCE) ||
				(m_type == SYM_FP_REL_SINGLE) || 
				(m_type == SYM_FP_REL_INT32);
		}

		bool IsConstant() const
		{
			return (m_type == SYM_CONSTANT) || (m_type == SYM_CONSTANT64);
		}

		bool IsTemporary() const
		{
			return 
				(m_type == SYM_TEMPORARY) || 
				(m_type == SYM_TEMPORARY64) || 
				(m_type == SYM_TEMPORARY128) || 
				(m_type == SYM_TEMPORARY256) ||
				(m_type == SYM_TMP_REFERENCE) ||
				(m_type == SYM_FP_TMP_SINGLE);
		}

		bool Equals(CSymbol* symbol) const
		{
			return 
				(symbol) &&
				(symbol->m_type == m_type) &&
				(symbol->m_valueLow == m_valueLow) &&
				(symbol->m_valueHigh == m_valueHigh);
		}

		bool Aliases(CSymbol* symbol) const
		{
			if(!IsRelative()) return false;
			if(!symbol->IsRelative()) return false;
			uint32 base1 = m_valueLow;
			uint32 base2 = symbol->m_valueLow;
//			uint32 end1 = base1 + GetSize();
//			uint32 end2 = base2 + symbol->GetSize();
			if(abs(static_cast<int32>(base2 - base1)) < GetSize()) return true;
			if(abs(static_cast<int32>(base2 - base1)) < symbol->GetSize()) return true;
			return false;
		}

		SYM_TYPE				m_type;
		uint32					m_valueLow;
		uint32					m_valueHigh;

		unsigned int			m_useCount;
		unsigned int			m_firstUse;
		unsigned int			m_firstDef;
		unsigned int			m_lastDef;
		unsigned int			m_rangeBegin;
		unsigned int			m_rangeEnd;
		unsigned int			m_stackLocation;
		bool					m_aliased;

		unsigned int			m_regAlloc_register;
		unsigned int			m_regAlloc_loadBeforeIdx;
		unsigned int			m_regAlloc_saveAfterIdx;
		unsigned int			m_regAlloc_notAllocatedAfterIdx;
	};

	typedef std::shared_ptr<CSymbol> SymbolPtr;
	typedef std::weak_ptr<CSymbol> WeakSymbolPtr;
}

#endif

#include "Jitter_CodeGenFactory.h"

#ifdef WIN32

	#ifdef _M_X64
		#include "Jitter_CodeGen_x86_64.h"
	#else
		#include "Jitter_CodeGen_x86_32.h"
	#endif

#elif defined(__APPLE__)

	#include <TargetConditionals.h>
	#if TARGET_CPU_ARM
		#include "Jitter_CodeGen_Arm.h"
	#elif TARGET_CPU_X86
		#include "Jitter_CodeGen_x86_32.h"
	#elif TARGET_CPU_X86_64
		#include "Jitter_CodeGen_x86_64.h"
	#else
		#warning Architecture not supported
	#endif

#elif defined(__ANDROID__)

	#include "Jitter_CodeGen_Arm.h"

#endif

Jitter::CCodeGen* Jitter::CreateCodeGen()
{
#ifdef WIN32
	
	#ifdef _M_X64
		return new Jitter::CCodeGen_x86_64();
	#else
		return new Jitter::CCodeGen_x86_32();
	#endif
	
#elif defined(__APPLE__)
	
	#if TARGET_CPU_ARM
		return new Jitter::CCodeGen_Arm();
	#elif TARGET_CPU_X86
		return new Jitter::CCodeGen_x86_32();
	#elif TARGET_CPU_X86_64
		return new Jitter::CCodeGen_x86_64();
	#else
		throw std::runtime_error("Unsupported architecture.");
	#endif

#elif defined(__ANDROID__)

	return new Jitter::CCodeGen_Arm();

#endif
}

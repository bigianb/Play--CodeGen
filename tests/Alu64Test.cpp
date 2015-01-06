#include "Alu64Test.h"
#include "MemStream.h"

void CAlu64Test::Run()
{
	memset(&m_context, 0, sizeof(m_context));

	m_context.value0 = 0xFEDCBA9876543210ULL;
	m_context.value1 = 0x012389AB4567CDEFULL;

	m_function(&m_context);

	TEST_VERIFY(m_context.resultAdd == 0x00004443BBBBFFFF);
	TEST_VERIFY(m_context.resultSub == m_context.value1);
}

void CAlu64Test::Compile(Jitter::CJitter& jitter)
{
	Framework::CMemStream codeStream;
	jitter.SetStream(&codeStream);

	jitter.Begin();
	{
		jitter.PushRel64(offsetof(CONTEXT, value0));
		jitter.PushRel64(offsetof(CONTEXT, value1));
		jitter.Add64();
		jitter.PullRel64(offsetof(CONTEXT, resultAdd));

		jitter.PushRel64(offsetof(CONTEXT, resultAdd));
		jitter.PushRel64(offsetof(CONTEXT, value0));
		jitter.Sub64();
		jitter.PullRel64(offsetof(CONTEXT, resultSub));
	}
	jitter.End();

	m_function = CMemoryFunction(codeStream.GetBuffer(), codeStream.GetSize());
}

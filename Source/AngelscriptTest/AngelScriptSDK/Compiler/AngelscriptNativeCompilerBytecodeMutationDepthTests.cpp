#include "Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCompilerBytecodeMutationDepthPrivate
{
	inline int32 CountLinkedInstructions(const asCByteCode& ByteCode)
	{
		int32 Count = 0;
		for (const asCByteInstruction* Instruction = const_cast<asCByteCode&>(ByteCode).GetFirstInstr();
			Instruction != nullptr;
			Instruction = Instruction->next)
		{
			++Count;
		}
		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerBytecodeMutationDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.MutationDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// Each cell owns an FBytecodeFixture because the raw asCByteCode builder and
	// module are intentionally destroyed together after every instruction mutation.
	TEST_METHOD(InstructionInsertionRemovalAndJumpResolutionPreserveLinks)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BYTECODE-MUTATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		for (int32 InstructionCount = 0; InstructionCount < 4; ++InstructionCount)
		{
			for (int32 MutationKind = 0; MutationKind < 4; ++MutationKind)
			{
				const FString FixtureName = FString::Printf(TEXT("CompilerBytecodeMutation_%d_%d"), InstructionCount, MutationKind);
				FTCHARToUTF8 FixtureNameUtf8(*FixtureName);
				FBytecodeFixture Fixture(FixtureNameUtf8.Get(), true);
				if (!Fixture.IsValid(*TestRunner))
				{
					continue;
				}

				for (int32 Index = 0; Index < InstructionCount; ++Index)
				{
					Fixture.ByteCode->InstrDWORD(asBC_PshC4, static_cast<asDWORD>(Index + 1));
				}

				if (MutationKind == 1)
				{
					Fixture.ByteCode->InstrDWORD(asBC_JMP, 1);
					Fixture.ByteCode->InstrDWORD(asBC_PshC4, 99);
					Fixture.ByteCode->Label(1);
				}
				else if (MutationKind == 2)
				{
					Fixture.ByteCode->InstrDWORD(asBC_PshC4, 77);
					ASSERT_THAT(AreEqual(0, Fixture.ByteCode->RemoveLastInstr(),
						TEXT("Removing the appended instruction should report success")));
				}
				else if (MutationKind == 3)
				{
					Fixture.ByteCode->InstrDWORD(asBC_PshC4, 33);
					Fixture.ByteCode->InstrDWORD(asBC_PshC4, 44);
					Fixture.ByteCode->Optimize();
				}

				if (MutationKind == 1)
				{
					ASSERT_THAT(AreEqual(0, Fixture.ByteCode->ResolveJumpAddresses(),
						TEXT("A labelled jump should resolve before link inspection")));
				}

				const int32 LinkedCount = AngelscriptCompilerBytecodeMutationDepthPrivate::CountLinkedInstructions(*Fixture.ByteCode);
				ASSERT_THAT(AreEqual(LinkedCount, CountInstructions(*Fixture.ByteCode),
					TEXT("Instruction traversal should agree with the bytecode count after mutation")));
				if (LinkedCount > 0)
				{
					ASSERT_THAT(IsNotNull(Fixture.ByteCode->GetFirstInstr(), TEXT("Non-empty bytecode should retain a head")));
					ASSERT_THAT(AreEqual(Fixture.ByteCode->GetSize(), EmitToBuffer(*Fixture.ByteCode).Num(),
						TEXT("Serialized size should remain synchronized after mutation")));
				}
				else
				{
					ASSERT_THAT(IsNull(Fixture.ByteCode->GetFirstInstr(), TEXT("An empty mutation should clear the head")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS

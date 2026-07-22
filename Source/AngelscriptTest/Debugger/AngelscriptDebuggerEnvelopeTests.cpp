#include "CQTest.h"
#include "Debugging/AngelscriptDebugServer.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptDebuggerEnvelopeTests_Private
{
	bool TryReadEnvelope(
		FAutomationTestBase& Test,
		TArray<uint8>& Buffer,
		bool bExpectedEnvelope,
		FAngelscriptDebugMessageEnvelope* OutEnvelope = nullptr)
	{
		FAngelscriptDebugMessageEnvelope Envelope;
		bool bHasEnvelope = false;
		FString Error;
		if (!Test.TestTrue(TEXT("Debugger.Protocol should accept a valid or incomplete envelope buffer"), TryDeserializeDebugMessageEnvelope(Buffer, Envelope, bHasEnvelope, &Error)))
		{
			Test.AddError(Error);
			return false;
		}

		if (!Test.TestEqual(TEXT("Debugger.Protocol should report envelope completeness accurately"), bHasEnvelope, bExpectedEnvelope))
		{
			return false;
		}

		if (bHasEnvelope && OutEnvelope != nullptr)
		{
			*OutEnvelope = MoveTemp(Envelope);
		}

		return true;
	}

	TArray<uint8> MakeInvalidLengthBuffer(int32 MessageLength)
	{
		TArray<uint8> Buffer;
		FMemoryWriter Writer(Buffer);
		Writer << MessageLength;
		return Buffer;
	}
}

TEST_CLASS_WITH_FLAGS(
	FAngelscriptDebuggerEnvelopeTests,
	"Angelscript.TestModule.Debugger.Protocol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IncrementalEnvelopeAssembly)
	{
		const TArray<uint8> FirstBody = { 0x11, 0x22, 0x33 };
		TArray<uint8> FirstEnvelopeBytes;
		ASSERT_THAT(IsTrue(SerializeDebugMessageEnvelope(EDebugMessageType::Pause, FirstBody, FirstEnvelopeBytes)));
		ASSERT_THAT(IsTrue(FirstEnvelopeBytes.Num() > static_cast<int32>(sizeof(int32) + sizeof(uint8))));

		TArray<uint8> ReceiveBuffer;
		ReceiveBuffer.Append(FirstEnvelopeBytes.GetData(), 2);
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, false)));

		ReceiveBuffer.Append(FirstEnvelopeBytes.GetData() + 2, static_cast<int32>(sizeof(int32)) - 2);
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, false)));

		ReceiveBuffer.Append(FirstEnvelopeBytes.GetData() + sizeof(int32), 1);
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, false)));

		ReceiveBuffer.Append(
			FirstEnvelopeBytes.GetData() + sizeof(int32) + 1,
			FirstEnvelopeBytes.Num() - static_cast<int32>(sizeof(int32) + 1));
		FAngelscriptDebugMessageEnvelope FirstEnvelope;
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, true, &FirstEnvelope)));
		ASSERT_THAT(AreEqual(EDebugMessageType::Pause, FirstEnvelope.MessageType));
		ASSERT_THAT(AreEqual(FirstBody, FirstEnvelope.Body));
		ASSERT_THAT(AreEqual(0, ReceiveBuffer.Num()));

		const TArray<uint8> SecondBody = { 0x44 };
		TArray<uint8> SecondEnvelopeBytes;
		ASSERT_THAT(IsTrue(SerializeDebugMessageEnvelope(EDebugMessageType::Continue, SecondBody, SecondEnvelopeBytes)));

		ReceiveBuffer.Append(FirstEnvelopeBytes);
		ReceiveBuffer.Append(SecondEnvelopeBytes);
		FAngelscriptDebugMessageEnvelope ReassembledFirstEnvelope;
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, true, &ReassembledFirstEnvelope)));
		ASSERT_THAT(AreEqual(EDebugMessageType::Pause, ReassembledFirstEnvelope.MessageType));
		ASSERT_THAT(IsTrue(ReceiveBuffer.Num() > 0));

		FAngelscriptDebugMessageEnvelope ReassembledSecondEnvelope;
		ASSERT_THAT(IsTrue(AngelscriptDebuggerEnvelopeTests_Private::TryReadEnvelope(*TestRunner, ReceiveBuffer, true, &ReassembledSecondEnvelope)));
		ASSERT_THAT(AreEqual(EDebugMessageType::Continue, ReassembledSecondEnvelope.MessageType));
		ASSERT_THAT(AreEqual(SecondBody, ReassembledSecondEnvelope.Body));
		ASSERT_THAT(AreEqual(0, ReceiveBuffer.Num()));
	}

	TEST_METHOD(InvalidLengthIsRejected)
	{
		TArray<uint8> ReceiveBuffer = AngelscriptDebuggerEnvelopeTests_Private::MakeInvalidLengthBuffer(0);
		FAngelscriptDebugMessageEnvelope Envelope;
		bool bHasEnvelope = false;
		FString Error;
		ASSERT_THAT(IsFalse(TryDeserializeDebugMessageEnvelope(ReceiveBuffer, Envelope, bHasEnvelope, &Error)));
		ASSERT_THAT(IsFalse(bHasEnvelope));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("invalid message length"), ESearchCase::IgnoreCase)));

		ReceiveBuffer = AngelscriptDebuggerEnvelopeTests_Private::MakeInvalidLengthBuffer(17 * 1024 * 1024);
		Error.Reset();
		ASSERT_THAT(IsFalse(TryDeserializeDebugMessageEnvelope(ReceiveBuffer, Envelope, bHasEnvelope, &Error)));
		ASSERT_THAT(IsFalse(bHasEnvelope));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("invalid message length"), ESearchCase::IgnoreCase)));
	}
};

#endif

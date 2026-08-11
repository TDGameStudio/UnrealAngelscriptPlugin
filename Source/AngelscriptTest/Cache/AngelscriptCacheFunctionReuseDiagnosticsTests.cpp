#include "Cache/AngelscriptCacheCompileReuse.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionReuseDiagnosticsTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bytes); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionReuseDiagnosticsTests,
	"Angelscript.TestModule.Cache.FunctionReuseDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyServiceExposesAnAbsentFunctionReuseSummary)
	{
		FAngelscriptCacheService Service;
		const FAngelscriptCacheDiagnosticSnapshot Snapshot =
			Service.CaptureDiagnosticSnapshot();

		ASSERT_THAT(AreEqual(uint32(4), Snapshot.SchemaVersion));
		ASSERT_THAT(IsFalse(Snapshot.FunctionReuse.bPresent));

		FString Json;
		ASSERT_THAT(IsTrue(
			SerializeAngelscriptCacheDiagnosticSnapshotJson(Snapshot, Json)));
		ASSERT_THAT(IsTrue(Json.Contains(
			TEXT("\"functionReuse\":{\"present\":false}"))));
		ASSERT_THAT(IsFalse(Json.Contains(TEXT("functionId"))));
	}

	TEST_METHOD(PublishedFunctionReuseSummaryIsStablePointerFreeAndResettable)
	{
		using namespace AngelscriptCacheFunctionReuseDiagnosticsTests_Private;
		FAngelscriptCacheService Service;
		FAngelscriptCacheFunctionReuseSummary Summary;
		Summary.bPresent = true;
		Summary.CandidateGenerationId = MakeHash(17);
		Summary.CandidateModuleCount = 3;
		Summary.RestoredFunctionCount = 19;
		Summary.CompiledMissCount = 4;
		Summary.NotCacheableCount = 2;
		Summary.RejectedCorruptCount = 1;
		Service.PublishFunctionReuseSummary(Summary);

		const FAngelscriptCacheDiagnosticSnapshot Snapshot =
			Service.CaptureDiagnosticSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.FunctionReuse.bPresent));
		ASSERT_THAT(IsTrue(
			Snapshot.FunctionReuse.CandidateGenerationId
				== Summary.CandidateGenerationId));
		ASSERT_THAT(AreEqual(uint32(3),
			Snapshot.FunctionReuse.CandidateModuleCount));
		ASSERT_THAT(AreEqual(uint32(19),
			Snapshot.FunctionReuse.RestoredFunctionCount));
		ASSERT_THAT(AreEqual(uint32(4),
			Snapshot.FunctionReuse.CompiledMissCount));
		ASSERT_THAT(AreEqual(uint32(2),
			Snapshot.FunctionReuse.NotCacheableCount));
		ASSERT_THAT(AreEqual(uint32(1),
			Snapshot.FunctionReuse.RejectedCorruptCount));

		FString JsonA;
		FString JsonB;
		ASSERT_THAT(IsTrue(
			SerializeAngelscriptCacheDiagnosticSnapshotJson(Snapshot, JsonA)));
		ASSERT_THAT(IsTrue(
			SerializeAngelscriptCacheDiagnosticSnapshotJson(Snapshot, JsonB)));
		ASSERT_THAT(IsTrue(JsonA == JsonB));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			*Summary.CandidateGenerationId.ToHexString())));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"candidateModuleCount\":3"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"restoredFunctionCount\":19"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"compiledMissCount\":4"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"notCacheableCount\":2"))));
		ASSERT_THAT(IsTrue(JsonA.Contains(
			TEXT("\"rejectedCorruptCount\":1"))));
		ASSERT_THAT(IsFalse(JsonA.Contains(TEXT("functionId"))));

		Service.ClearFunctionReuseSummary();
		ASSERT_THAT(IsFalse(
			Service.CaptureDiagnosticSnapshot().FunctionReuse.bPresent));
	}
};

#endif

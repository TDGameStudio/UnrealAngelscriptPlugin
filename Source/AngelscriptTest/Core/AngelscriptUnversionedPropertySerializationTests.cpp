#include "Core/AngelscriptUnversionedPropertySerializationTestTypes.h"

#include "UnversionedPropertySerialization.h"

#include "CQTest.h"
#include "Hash/Blake3.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "Templates/GuardValueAccessors.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptUnversionedPropertySerializationTests,
	"Angelscript.TestModule.Engine.UnversionedPropertySerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
enum class ESerializationPath : uint8
{
	Versioned,
	Unversioned,
};

class FAngelscriptUnversionedPropertyTestArchive final : public FArchiveProxy
{
public:
	using FArchiveProxy::FArchiveProxy;

	virtual FArchive& operator<<(FName& Value) override
	{
		uint32 UnstableDisplayIndex = Value.GetDisplayIndex().ToUnstableInt();
		int32 Number = Value.GetNumber();
		InnerArchive << UnstableDisplayIndex << Number;

		if (IsLoading())
		{
			Value = FName::CreateFromDisplayId(FNameEntryId::FromUnstableInt(UnstableDisplayIndex), Number);
		}

		return *this;
	}
};

static const TCHAR* ToString(const ESerializationPath Path)
{
	return Path == ESerializationPath::Unversioned ? TEXT("unversioned") : TEXT("versioned");
}

static UScriptStruct* GetFixtureStruct()
{
	return FAngelscriptUnversionedPropertySerializationFixture::StaticStruct();
}

static bool IsUnversionedPropertySerializationEnabled()
{
	bool bEnabled = false;
	return GConfig->GetBool(TEXT("Core.System"), TEXT("CanUseUnversionedPropertySerialization"), bEnabled, GEngineIni) && bEnabled;
}

static FAngelscriptUnversionedPropertySerializationFixture MakeNonDefaultFixture()
{
	FAngelscriptUnversionedPropertySerializationFixture Fixture;
	Fixture.Count = 97;
	Fixture.bEnabled = true;
	Fixture.Label = TEXT("RoundTripFixture");
	Fixture.Values = { 7, 0, 42, -9 };
	return Fixture;
}

static bool ExpectFixtureEquals(
	FAutomationTestBase& Test,
	const TCHAR* Context,
	const FAngelscriptUnversionedPropertySerializationFixture& Actual,
	const FAngelscriptUnversionedPropertySerializationFixture& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	bool bOk = true;
	bOk &= LocalAssert.AreEqual(
		Expected.Count,
		Actual.Count,
		FString::Printf(TEXT("%s should preserve Count"), Context));
	bOk &= LocalAssert.AreEqual(
		Expected.bEnabled,
		Actual.bEnabled,
		FString::Printf(TEXT("%s should preserve bEnabled"), Context));
	bOk &= LocalAssert.AreEqual(
		Expected.Label,
		Actual.Label,
		FString::Printf(TEXT("%s should preserve Label"), Context));
	bOk &= LocalAssert.AreEqual(
		Expected.Values.Num(),
		Actual.Values.Num(),
		FString::Printf(TEXT("%s should preserve Values.Num"), Context));

	const int32 SharedValueCount = FMath::Min(Actual.Values.Num(), Expected.Values.Num());
	for (int32 Index = 0; Index < SharedValueCount; ++Index)
	{
		bOk &= LocalAssert.AreEqual(
			Expected.Values[Index],
			Actual.Values[Index],
			FString::Printf(TEXT("%s should preserve Values[%d]"), Context, Index));
	}

	return bOk;
}

static void SaveFixture(
	const ESerializationPath Path,
	const FAngelscriptUnversionedPropertySerializationFixture& Source,
	const FAngelscriptUnversionedPropertySerializationFixture& Defaults,
	TArray<uint8>& OutBytes)
{
	FAngelscriptUnversionedPropertySerializationFixture MutableSource = Source;
	FAngelscriptUnversionedPropertySerializationFixture MutableDefaults = Defaults;
	UScriptStruct* Struct = GetFixtureStruct();

	OutBytes.Reset();

	FMemoryWriter Writer(OutBytes);
	Writer.SetUseUnversionedPropertySerialization(Path == ESerializationPath::Unversioned);

	FAngelscriptUnversionedPropertyTestArchive Linker(Writer);
	FBinaryArchiveFormatter Formatter(Linker);
	FStructuredArchive StructuredArchive(Formatter);
	FStructuredArchive::FSlot Slot = StructuredArchive.Open();

	if (Path == ESerializationPath::Unversioned)
	{
		SerializeUnversionedProperties(
			Struct,
			Slot,
			reinterpret_cast<uint8*>(&MutableSource),
			Struct,
			reinterpret_cast<uint8*>(&MutableDefaults));
	}
	else
	{
		Struct->SerializeTaggedProperties(
			Slot,
			reinterpret_cast<uint8*>(&MutableSource),
			Struct,
			reinterpret_cast<uint8*>(&MutableDefaults));
	}
}

static bool LoadFixture(
	FAutomationTestBase& Test,
	const TCHAR* Context,
	const ESerializationPath Path,
	const TArray<uint8>& Bytes,
	const FAngelscriptUnversionedPropertySerializationFixture& Defaults,
	FAngelscriptUnversionedPropertySerializationFixture& OutLoaded)
{
	FAngelscriptUnversionedPropertySerializationFixture MutableDefaults = Defaults;
	UScriptStruct* Struct = GetFixtureStruct();

	FMemoryReader Reader(Bytes);
	Reader.SetUseUnversionedPropertySerialization(Path == ESerializationPath::Unversioned);

	FAngelscriptUnversionedPropertyTestArchive Linker(Reader);
	FBinaryArchiveFormatter Formatter(Linker);
	FStructuredArchive StructuredArchive(Formatter);
	FStructuredArchive::FSlot Slot = StructuredArchive.Open();

	TGuardValueAccessors<bool> SavingPackageGuard([]() { return UE::IsSavingPackage(); }, UE::SetIsSavingPackage, false);
	OutLoaded = FAngelscriptUnversionedPropertySerializationFixture();

	if (Path == ESerializationPath::Unversioned)
	{
		SerializeUnversionedProperties(
			Struct,
			Slot,
			reinterpret_cast<uint8*>(&OutLoaded),
			Struct,
			reinterpret_cast<uint8*>(&MutableDefaults));
	}
	else
	{
		Struct->UStruct::SerializeTaggedProperties(
			Slot,
			reinterpret_cast<uint8*>(&OutLoaded),
			Struct,
			reinterpret_cast<uint8*>(&MutableDefaults));
	}

	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(
		static_cast<int64>(Bytes.Num()),
		Reader.Tell(),
		FString::Printf(TEXT("%s should consume the entire %s payload"), Context, ToString(Path)));
}

static bool ExpectRoundTrip(
	FAutomationTestBase& Test,
	const TCHAR* Context,
	const ESerializationPath Path,
	const FAngelscriptUnversionedPropertySerializationFixture& Source,
	const FAngelscriptUnversionedPropertySerializationFixture& Defaults)
{
	TArray<uint8> Payload;
	SaveFixture(Path, Source, Defaults, Payload);

	FNoDiscardAsserter LocalAssert(Test);
	bool bOk = LocalAssert.IsTrue(
		Payload.Num() > 0,
		FString::Printf(TEXT("%s should produce a non-empty %s payload"), Context, ToString(Path)));

	FAngelscriptUnversionedPropertySerializationFixture Loaded;
	bOk &= LoadFixture(Test, Context, Path, Payload, Defaults, Loaded);
	bOk &= ExpectFixtureEquals(Test, Context, Loaded, Source);
	return bOk;
}

public:
	TEST_METHOD(RoundTripsAndRebuildsSchemaCache)
	{
ASSERT_THAT(IsTrue(
			IsUnversionedPropertySerializationEnabled(),
			TEXT("Automation environment should enable unversioned property serialization")));

		UScriptStruct* Struct = GetFixtureStruct();
		DestroyAngelscriptUnversionedSchema(Struct);
		ON_SCOPE_EXIT
		{
			DestroyAngelscriptUnversionedSchema(Struct);
		};

		const FAngelscriptUnversionedPropertySerializationFixture Defaults;
		const FAngelscriptUnversionedPropertySerializationFixture NonDefault = MakeNonDefaultFixture();

		ExpectRoundTrip(*TestRunner, TEXT("Versioned default fixture"), ESerializationPath::Versioned, Defaults, Defaults);
		ExpectRoundTrip(*TestRunner, TEXT("Versioned non-default fixture"), ESerializationPath::Versioned, NonDefault, Defaults);
		ExpectRoundTrip(*TestRunner, TEXT("Unversioned default fixture"), ESerializationPath::Unversioned, Defaults, Defaults);
		ExpectRoundTrip(*TestRunner, TEXT("Unversioned non-default fixture"), ESerializationPath::Unversioned, NonDefault, Defaults);

#if WITH_EDITORONLY_DATA
		const FBlake3Hash SchemaHashBeforeDestroy = GetSchemaHash(Struct, false);
#endif

		DestroyAngelscriptUnversionedSchema(Struct);

#if WITH_EDITORONLY_DATA
		const FBlake3Hash SchemaHashAfterDestroy = GetSchemaHash(Struct, false);
		ASSERT_THAT(IsTrue(
			SchemaHashBeforeDestroy == SchemaHashAfterDestroy,
			TEXT("Schema hash should stay stable across cache destruction")));
#endif

		ExpectRoundTrip(*TestRunner, TEXT("Unversioned default fixture after schema rebuild"), ESerializationPath::Unversioned, Defaults, Defaults);
		ExpectRoundTrip(*TestRunner, TEXT("Unversioned non-default fixture after schema rebuild"), ESerializationPath::Unversioned, NonDefault, Defaults);
	}
};

#endif

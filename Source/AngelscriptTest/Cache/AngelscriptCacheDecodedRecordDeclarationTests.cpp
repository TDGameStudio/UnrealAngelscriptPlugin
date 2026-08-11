#include "Cache/Private/AngelscriptCacheTypeSchemaCodec.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheDecodedRecordDeclarationTests_Private
{
	template <typename Type>
	concept CHasPublicRecordVariant = requires
	{
		typename Type::FRecordVariant;
	};

	template <typename Type>
	concept CHasPublicTypeSchemaOffsetStorage = requires
	{
		typename Type::FTypeSchemaCapturedOffsetStorage;
	};

	template <typename Type>
	concept CHasPublicSourceIndexAlternative = requires
	{
		typename Type::FSourceIndexRecord;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDecodedRecordDeclarationTests,
	"Angelscript.TestModule.Cache.Archive.DecodedRecordDeclaration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FinalStorageIsCxxPrivateAndCodecBridgeIsNonOwning)
	{
		using namespace AngelscriptCacheDecodedRecordDeclarationTests_Private;

		static_assert(!CHasPublicRecordVariant<FAngelscriptDecodedCacheRecord>);
		static_assert(!CHasPublicTypeSchemaOffsetStorage<FAngelscriptDecodedCacheRecord>);
		static_assert(!CHasPublicSourceIndexAlternative<FAngelscriptDecodedCacheRecord>);
		static_assert(std::is_empty_v<
			AngelscriptCacheTypeSchema_Private::FDecodedRecordCodecBridge>);
		static_assert(!std::is_default_constructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_copy_constructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_move_constructible_v<FAngelscriptDecodedCacheRecord>);

		ASSERT_THAT(IsTrue(std::is_empty_v<
			AngelscriptCacheTypeSchema_Private::FDecodedRecordCodecBridge>));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS

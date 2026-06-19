// ============================================================================
// AngelscriptCollisionProfileBindingsTests.cpp
//
// UCollisionProfile conversion binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.CollisionProfile.FAngelscriptCollisionProfileBindingsTest.*
//
// Sections:
//   ObjectTypeConversion — ConvertToObjectType + round-trip via ConvertToCollisionChannel
//                          for WorldStatic and WorldDynamic
//   TraceTypeConversion  — ConvertToTraceType + round-trip for Visibility and Camera,
//                          plus composite round-trip cases
//
// Native baselines are computed in each TEST_METHOD and injected into AS via
// $TOKEN$ substitution (ReplaceInline), since the enum-to-int mapping is
// platform/project dependent.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


TEST_CLASS_WITH_FLAGS(FAngelscriptCollisionProfileBindingsTest,
	"Angelscript.TestModule.Bindings.CollisionProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ObjectTypeConversion
	// ====================================================================

	TEST_METHOD(ObjectTypeConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compute native baselines
		UCollisionProfile* CP = UCollisionProfile::Get();
		if (!this->Assert.IsNotNull(CP, TEXT("CollisionProfile singleton should be available")))
		{
			return;
		}

		const int32 WorldStaticObjType = static_cast<int32>(CP->ConvertToObjectType(ECC_WorldStatic));
		const int32 WorldDynamicObjType = static_cast<int32>(CP->ConvertToObjectType(ECC_WorldDynamic));
		const int32 WorldStaticChannel = static_cast<int32>(ECC_WorldStatic);
		const int32 WorldDynamicChannel = static_cast<int32>(ECC_WorldDynamic);

		FString Script = TEXT(R"(
int ObjType_WorldStatic()
{
	return int(UCollisionProfile::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
}

int ObjType_WorldDynamic()
{
	return int(UCollisionProfile::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
}

int ObjType_WorldStaticRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(false, $WORLD_STATIC_OBJ_TYPE$));
}

int ObjType_WorldDynamicRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(false, $WORLD_DYNAMIC_OBJ_TYPE$));
}
)");

		Script.ReplaceInline(TEXT("$WORLD_STATIC_OBJ_TYPE$"), *LexToString(WorldStaticObjType));
		Script.ReplaceInline(TEXT("$WORLD_DYNAMIC_OBJ_TYPE$"), *LexToString(WorldDynamicObjType));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionProfile_ObjType"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ObjType_WorldStatic()"),
			TEXT("ConvertToObjectType(WorldStatic) should match native baseline"),
			WorldStaticObjType);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ObjType_WorldDynamic()"),
			TEXT("ConvertToObjectType(WorldDynamic) should match native baseline"),
			WorldDynamicObjType);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ObjType_WorldStaticRoundTrip()"),
			TEXT("WorldStatic object type should round-trip back to ECC_WorldStatic"),
			WorldStaticChannel);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ObjType_WorldDynamicRoundTrip()"),
			TEXT("WorldDynamic object type should round-trip back to ECC_WorldDynamic"),
			WorldDynamicChannel);
	}

	// ====================================================================
	// Section: TraceTypeConversion
	// ====================================================================

	TEST_METHOD(TraceTypeConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compute native baselines
		UCollisionProfile* CP = UCollisionProfile::Get();
		if (!this->Assert.IsNotNull(CP, TEXT("CollisionProfile singleton should be available")))
		{
			return;
		}

		const int32 VisibilityTraceType = static_cast<int32>(CP->ConvertToTraceType(ECC_Visibility));
		const int32 CameraTraceType = static_cast<int32>(CP->ConvertToTraceType(ECC_Camera));
		const int32 VisibilityChannel = static_cast<int32>(ECC_Visibility);
		const int32 CameraChannel = static_cast<int32>(ECC_Camera);
		const int32 WorldStaticChannel = static_cast<int32>(ECC_WorldStatic);

		FString Script = TEXT(R"(
int TraceType_Visibility()
{
	return int(UCollisionProfile::ConvertToTraceType(ECollisionChannel::ECC_Visibility));
}

int TraceType_Camera()
{
	return int(UCollisionProfile::ConvertToTraceType(ECollisionChannel::ECC_Camera));
}

int TraceType_VisibilityRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(true, $VISIBILITY_TRACE_TYPE$));
}

int TraceType_CameraRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(true, $CAMERA_TRACE_TYPE$));
}

int Composite_ObjTypeRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(false, int(UCollisionProfile::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic))));
}

int Composite_TraceTypeRoundTrip()
{
	return int(UCollisionProfile::ConvertToCollisionChannel(true, int(UCollisionProfile::ConvertToTraceType(ECollisionChannel::ECC_Visibility))));
}
)");

		Script.ReplaceInline(TEXT("$VISIBILITY_TRACE_TYPE$"), *LexToString(VisibilityTraceType));
		Script.ReplaceInline(TEXT("$CAMERA_TRACE_TYPE$"), *LexToString(CameraTraceType));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionProfile_TraceType"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int TraceType_Visibility()"),
			TEXT("ConvertToTraceType(Visibility) should match native baseline"),
			VisibilityTraceType);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int TraceType_Camera()"),
			TEXT("ConvertToTraceType(Camera) should match native baseline"),
			CameraTraceType);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int TraceType_VisibilityRoundTrip()"),
			TEXT("Visibility trace type should round-trip back to ECC_Visibility"),
			VisibilityChannel);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int TraceType_CameraRoundTrip()"),
			TEXT("Camera trace type should round-trip back to ECC_Camera"),
			CameraChannel);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Composite_ObjTypeRoundTrip()"),
			TEXT("Composite WorldStatic object-type round-trip should return ECC_WorldStatic"),
			WorldStaticChannel);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Composite_TraceTypeRoundTrip()"),
			TEXT("Composite Visibility trace-type round-trip should return ECC_Visibility"),
			VisibilityChannel);
	}
};

#endif

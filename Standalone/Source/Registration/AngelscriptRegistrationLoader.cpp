#include "Registration/AngelscriptRegistrationLoader.h"

#include "Adapters/AngelscriptAdapterRegistry.h"
#include "Adapters/AngelscriptTemplateTraits.h"
#include "Registration/AngelscriptCompileOnlyStub.h"

#include "angelscript.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AngelscriptStandalone
{
	class FCompileOnlyStringFactory final : public asIStringFactory
	{
	public:
		explicit FCompileOnlyStringFactory(
			const std::size_t InStorageSize)
			: StorageSize(std::max<std::size_t>(InStorageSize, 1))
		{
		}

		const void* GetStringConstant(
			const char* Data,
			const asUINT Length) override
		{
			auto Constant = std::make_unique<FStringConstant>();
			Constant->Storage.resize(StorageSize);
			Constant->Value.assign(Data, Length);
			const void* Address = Constant->Storage.data();
			Constants.emplace(Address, std::move(Constant));
			return Address;
		}

		int ReleaseStringConstant(const void* String) override
		{
			return Constants.erase(String) == 1 ? 0 : asERROR;
		}

		int GetRawStringData(
			const void* String,
			char* Data,
			asUINT* Length) const override
		{
			const auto Found = Constants.find(String);
			if (Found == Constants.end())
			{
				return asERROR;
			}
			if (Length != nullptr)
			{
				*Length = static_cast<asUINT>(
					Found->second->Value.size());
			}
			if (Data != nullptr)
			{
				std::memcpy(
					Data,
					Found->second->Value.data(),
					Found->second->Value.size());
			}
			return 0;
		}

	private:
		struct FStringConstant
		{
			std::vector<std::uint8_t> Storage;
			std::string Value;
		};

		std::size_t StorageSize;
		std::unordered_map<
			const void*,
			std::unique_ptr<FStringConstant>> Constants;
	};

	class FRegistrationBackingStore
	{
	public:
		std::deque<std::array<std::uint8_t, 64>> GlobalStorage;
		std::unique_ptr<FCompileOnlyStringFactory> StringFactory;
	};

	namespace
	{
		std::string_view GetAvailability(
			const FOfflineSymbolRecord& Symbol)
		{
			if (Symbol.Kind == "type")
			{
				return Symbol.Type.Availability;
			}
			if (Symbol.Kind == "callable")
			{
				return Symbol.Callable.Availability;
			}
			if (Symbol.Kind == "property")
			{
				return Symbol.Property.Availability;
			}
			return {};
		}

		class FNamespaceScope
		{
		public:
			FNamespaceScope(
				asIScriptEngine* InEngine,
				const std::string_view Namespace)
				: Engine(InEngine)
				, Previous(InEngine->GetDefaultNamespace())
			{
				Engine->SetDefaultNamespace(
					std::string(Namespace).c_str());
			}

			~FNamespaceScope()
			{
				Engine->SetDefaultNamespace(Previous.c_str());
			}

		private:
			asIScriptEngine* Engine;
			std::string Previous;
		};

		struct FRegisteredTypeName
		{
			std::string Namespace;
			std::string Name;
			bool bImplicitHandle = false;
		};

		std::string NormalizeImplicitHandleArraySyntax(
			const std::string_view Declaration,
			const std::set<std::string>& ImplicitHandleTypes)
		{
			std::string Result;
			Result.reserve(Declaration.size());
			for (std::size_t Offset = 0;
				Offset < Declaration.size();)
			{
				const unsigned char Character =
					static_cast<unsigned char>(Declaration[Offset]);
				if (!std::isalpha(Character)
					&& Declaration[Offset] != '_')
				{
					Result.push_back(Declaration[Offset++]);
					continue;
				}
				const std::size_t Begin = Offset;
				while (Offset < Declaration.size())
				{
					const unsigned char Current =
						static_cast<unsigned char>(Declaration[Offset]);
					if (std::isalnum(Current)
						|| Declaration[Offset] == '_'
						|| Declaration[Offset] == ':')
					{
						++Offset;
						continue;
					}
					break;
				}
				const std::string Token(
					Declaration.substr(Begin, Offset - Begin));
				std::size_t Next = Offset;
				while (Next < Declaration.size()
					&& std::isspace(static_cast<unsigned char>(
						Declaration[Next])))
				{
					++Next;
				}
				const std::size_t Qualified =
					Token.rfind("::");
				const std::string_view Unqualified =
					Qualified == std::string::npos
						? std::string_view(Token)
						: std::string_view(Token).substr(
							Qualified + 2);
				const bool bImplicitHandleArray =
					Next + 1 < Declaration.size()
					&& Declaration[Next] == '['
					&& Declaration[Next + 1] == ']'
					&& (ImplicitHandleTypes.contains(Token)
						|| ImplicitHandleTypes.contains(
							std::string(Unqualified)));
				if (bImplicitHandleArray)
				{
					Result += "TArray<";
					Result += Token;
					Result.push_back('>');
					Offset = Next + 2;
				}
				else
				{
					Result += Token;
				}
			}
			return Result;
		}

		std::string NormalizeFloatRegistrationSyntax(
			const std::string_view Declaration,
			const bool bFloatIsFloat64)
		{
			std::string Result;
			Result.reserve(Declaration.size() + 8);
			for (std::size_t Offset = 0;
				Offset < Declaration.size();)
			{
				const bool bFloat =
					Declaration.substr(Offset).starts_with("float")
					&& (Offset == 0
						|| (!std::isalnum(static_cast<unsigned char>(
								Declaration[Offset - 1]))
							&& Declaration[Offset - 1] != '_'))
					&& (Offset + 5 == Declaration.size()
						|| (!std::isalnum(static_cast<unsigned char>(
								Declaration[Offset + 5]))
							&& Declaration[Offset + 5] != '_'));
				if (bFloat)
				{
					Result += bFloatIsFloat64
						? "float64"
						: "float32";
					Offset += 5;
				}
				else
				{
					Result.push_back(Declaration[Offset++]);
				}
			}
			return Result;
		}

		std::string NormalizeObjectWrapperSubtypeSyntax(
			const std::string_view Declaration)
		{
			std::string Result;
			Result.reserve(Declaration.size() + 4);
			int TemplateDepth = 0;
			for (std::size_t Offset = 0;
				Offset < Declaration.size();)
			{
				if (Declaration[Offset] == '<')
				{
					++TemplateDepth;
					Result.push_back(Declaration[Offset++]);
					continue;
				}
				if (Declaration[Offset] == '>')
				{
					TemplateDepth = std::max(0, TemplateDepth - 1);
					Result.push_back(Declaration[Offset++]);
					continue;
				}
				const bool bDirectSubtype =
					Declaration[Offset] == 'T'
					&& (Offset == 0
						|| (!std::isalnum(static_cast<unsigned char>(
								Declaration[Offset - 1]))
							&& Declaration[Offset - 1] != '_'))
					&& (Offset + 1 == Declaration.size()
						|| (!std::isalnum(static_cast<unsigned char>(
								Declaration[Offset + 1]))
							&& Declaration[Offset + 1] != '_'))
					&& TemplateDepth == 0;
				if (bDirectSubtype)
				{
					Result += "T&";
					++Offset;
				}
				else
				{
					Result.push_back(Declaration[Offset++]);
				}
			}
			return Result;
		}

		std::string RemoveApplicationRegistrationQualifiers(
			const std::string_view Declaration)
		{
			std::string Result;
			Result.reserve(Declaration.size());
			for (std::size_t Offset = 0;
				Offset < Declaration.size();)
			{
				const bool bWordStart =
					std::isalpha(static_cast<unsigned char>(
						Declaration[Offset]))
					|| Declaration[Offset] == '_';
				if (!bWordStart)
				{
					Result.push_back(Declaration[Offset++]);
					continue;
				}
				const std::size_t Begin = Offset++;
				while (Offset < Declaration.size()
					&& (std::isalnum(static_cast<unsigned char>(
							Declaration[Offset]))
						|| Declaration[Offset] == '_'))
				{
					++Offset;
				}
				const std::string_view Word =
					Declaration.substr(Begin, Offset - Begin);
				const bool bApplicationOnlyQualifier =
					Word == "final"
					|| Word == "override"
					|| Word == "public"
					|| Word == "protected"
					|| Word == "private";
				if (!bApplicationOnlyQualifier)
				{
					Result.append(Word);
				}
			}
			while (!Result.empty()
				&& std::isspace(static_cast<unsigned char>(
					Result.back())))
			{
				Result.pop_back();
			}
			return Result;
		}

		std::string NormalizeRegistrationDeclaration(
			const std::string_view Declaration,
			const std::set<std::string>& ImplicitHandleTypes,
			const bool bFloatIsFloat64,
			const bool bObjectWrapperSurface = false)
		{
			std::string Result = RemoveApplicationRegistrationQualifiers(
				NormalizeFloatRegistrationSyntax(
					NormalizeImplicitHandleArraySyntax(
						Declaration,
						ImplicitHandleTypes),
					bFloatIsFloat64));
			if (bObjectWrapperSurface)
			{
				Result = NormalizeObjectWrapperSubtypeSyntax(Result);
			}
			return Result;
		}

		bool IsObjectWrapperAdapter(
			const std::string_view StableId)
		{
			const FKnownAdapter* Adapter = FindKnownAdapter(StableId);
			if (Adapter == nullptr)
			{
				return false;
			}
			const std::string_view Name = Adapter->Name;
			return Name == "TObjectPtr"
				|| Name == "TWeakObjectPtr"
				|| Name == "TSoftObjectPtr"
				|| Name == "TSubclassOf"
				|| Name == "TSoftClassPtr";
		}

		bool FailRegistration(
			FRegistrationLoadResult& Result,
			const FOfflineSymbolRecord& Symbol,
			std::string_view Operation,
			int Registration);

		constexpr asPWORD OfflineTypeTraitsUserDataId =
			static_cast<asPWORD>(0x4153545241495453ull);

		void AttachOfflineTypeTraits(
			asIScriptEngine* Engine,
			const int TypeId,
			const FOfflineTypeRecord& Type)
		{
			if (asITypeInfo* TypeInfo =
				Engine->GetTypeInfoById(TypeId))
			{
				TypeInfo->SetUserData(
					const_cast<FOfflineTypeRecord*>(&Type),
					OfflineTypeTraitsUserDataId);
			}
		}

		FTemplateTraits DeriveRuntimeTemplateTraits(
			asIScriptEngine* Engine,
			const asITypeInfo* Type,
			const int TypeId)
		{
			if (Type == nullptr)
			{
				FTemplateTraits Result;
				Result.bKnown = true;
				Result.bConstructible = true;
				Result.bDestructible = true;
				Result.bCopyable = true;
				Result.bComparable = true;
				Result.bHashable = true;
				Result.bTemplateEligible = true;
				Result.bValueType = true;
				const int Size =
					Engine->GetSizeOfPrimitiveType(TypeId);
				Result.ValueSize = Size > 0
					? static_cast<std::uint64_t>(Size)
					: 1;
				Result.ValueAlignment =
					std::min<std::uint64_t>(
						Result.ValueSize,
						8);
				return Result;
			}
			if (const auto* Offline =
				static_cast<const FOfflineTypeRecord*>(
					Type->GetUserData(
						OfflineTypeTraitsUserDataId)))
			{
				FTemplateTraits Result =
					DeriveTemplateTraits(*Offline);
				if ((TypeId & asTYPEID_OBJHANDLE) != 0)
					Result.bObjectHandleCompatible = true;
				return Result;
			}

			if (Type->GetSubTypeCount() != 0)
			{
				FTemplateTraits Result = DeriveAdapterInstanceTraits(
					Type->GetName(),
					Type->GetSize(),
					Type->alignment > 0
						? static_cast<std::uint64_t>(Type->alignment)
						: 0);
				if (Result.bKnown)
				{
					return Result;
				}
			}

			FTemplateTraits Result;
			const asQWORD Flags = Type->GetFlags();
			Result.bKnown = true;
			Result.bObjectReference = (Flags & asOBJ_REF) != 0;
			Result.bObjectHandleCompatible =
				Result.bObjectReference;
			Result.bValueType = (Flags & asOBJ_VALUE) != 0;
			Result.bConstructible = true;
			Result.bDestructible = true;
			Result.bCopyable = true;
			Result.bTemplateEligible = true;
			Result.bRequiresGarbageCollection =
				(Flags & asOBJ_GC) != 0;
			Result.ValueSize = Type->GetSize();
			Result.ValueAlignment =
				Type->alignment > 0
					? static_cast<std::uint64_t>(Type->alignment)
					: 0;
			return Result;
		}

		void TemplateValidationCallback(asIScriptGeneric* Generic)
		{
			asITypeInfo* TemplateType =
				*reinterpret_cast<asITypeInfo**>(
					Generic->GetAddressOfArg(0));
			bool bAccepted = TemplateType != nullptr;
			if (bAccepted)
			{
				const std::string_view Name(TemplateType->GetName());
				const asUINT ExpectedSubtypes =
					Name == "TMap" ? 2u : 1u;
				bAccepted =
					TemplateType->GetSubTypeCount()
						== ExpectedSubtypes;
				std::vector<FTemplateTraits> Traits;
				Traits.reserve(ExpectedSubtypes);
				for (asUINT Index = 0;
					bAccepted && Index < ExpectedSubtypes;
					++Index)
				{
					asITypeInfo* Subtype =
						TemplateType->GetSubType(Index);
					if (Subtype != nullptr
						&& Subtype->GetSubTypeCount() != 0
						&& !IsNestedTemplateAllowed(
							Name,
							Subtype->GetName()))
					{
						bAccepted = false;
						break;
					}
					Traits.push_back(
						DeriveRuntimeTemplateTraits(
							Generic->GetEngine(),
							Subtype,
							TemplateType->GetSubTypeId(Index)));
				}
				const FTemplateTraitValidation Validation =
					ValidateAdapterTemplateTraits(
						Name,
						Traits);
				bAccepted = bAccepted && Validation.bSuccess;
			}
			*reinterpret_cast<bool*>(
				Generic->GetAddressOfReturnLocation()) = bAccepted;
		}

		std::string TemplateUsageDeclaration(
			const std::string_view Name)
		{
			return Name.starts_with("TMap")
				? std::string(Name) + "<K,V>"
				: std::string(Name) + "<T>";
		}

		std::string TemplateRegistrationDeclaration(
			const std::string_view Name)
		{
			return Name.starts_with("TMap")
				? std::string(Name) + "<class K, class V>"
				: std::string(Name) + "<class T>";
		}

		bool RegisterAdapterTemplate(
			asIScriptEngine* Engine,
			const FOfflineSymbolRecord& Symbol,
			FRegistrationLoadResult& Result,
			std::map<std::string, FRegisteredTypeName>& OutNames)
		{
			const FOfflineTypeRecord& Type = Symbol.Type;
			const FKnownAdapter* Adapter =
				FindKnownAdapter(Type.AdapterStableId);
			if (Adapter == nullptr)
			{
				Result.Error =
					"template type references an unknown adapter: "
					+ Type.AdapterStableId;
				return false;
			}
			const bool bPrimaryAdapterTemplate =
				Type.Name == Adapter->Name;
			int ByteSize = Type.CompileSize > 0
				? static_cast<int>(Type.CompileSize)
				: 32;
			asQWORD Flags = asOBJ_VALUE
				| asOBJ_TEMPLATE
				| asOBJ_APP_CLASS_ALLINTS;
			if (bPrimaryAdapterTemplate)
			{
				Flags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
			}
			if (bPrimaryAdapterTemplate
				&& Adapter->Name == "TOptional")
			{
				ByteSize = 1;
				Flags |= asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE;
			}
			else if (bPrimaryAdapterTemplate
				&& (Adapter->Name == "TObjectPtr"
					|| Adapter->Name == "TWeakObjectPtr"
					|| Adapter->Name == "TSoftObjectPtr"
					|| Adapter->Name == "TSubclassOf"
					|| Adapter->Name == "TSoftClassPtr"))
			{
				ByteSize = static_cast<int>(sizeof(void*));
			}

			const int TypeId = Engine->RegisterObjectType(
				TemplateRegistrationDeclaration(Type.Name).c_str(),
				ByteSize,
				Flags);
			if (FailRegistration(
					Result,
					Symbol,
					"adapter template type registration",
					TypeId))
			{
				return false;
			}
			if (bPrimaryAdapterTemplate
				&& Type.Name == "TArray"
				&& Engine->RegisterDefaultArrayType("TArray<T>") < 0)
			{
				Result.Error =
					"failed to select TArray as the UE-validation "
					"default array type";
				return false;
			}
			const std::string Usage =
				TemplateUsageDeclaration(Type.Name);
			if (bPrimaryAdapterTemplate)
			{
				const int Callback = Engine->RegisterObjectBehaviour(
					Usage.c_str(),
					asBEHAVE_TEMPLATE_CALLBACK,
					"bool f(int&in Type, int&out ErrorMessage)",
					asFUNCTION(TemplateValidationCallback),
					asCALL_GENERIC);
				if (FailRegistration(
						Result,
						Symbol,
						"adapter template callback registration",
						Callback))
				{
					return false;
				}
			}
			Result.RuntimeMap.TypeIdByStableId.emplace(
				Symbol.StableId,
				TypeId);
			AttachOfflineTypeTraits(Engine, TypeId, Type);
			OutNames.emplace(
				Symbol.StableId,
				FRegisteredTypeName{
					Type.Namespace,
					Usage,
					false,
				});
			Result.Capabilities.push_back({
				Symbol.StableId,
				std::string(
					bPrimaryAdapterTemplate
						? "template-adapter:"
						: "template-support:")
					+ std::string(Adapter->Name)
					+ ":" + Type.Name,
				ECapabilityClassification::CompileShim,
				bPrimaryAdapterTemplate
					? "template traits and layout are standalone compile-only "
						"and explicitly non-UE-ABI"
					: "adapter support template preserves its exported "
						"declaration surface with validation-only layout",
			});
			return true;
		}

		bool FailRegistration(
			FRegistrationLoadResult& Result,
			const FOfflineSymbolRecord& Symbol,
			const std::string_view Operation,
			const int Registration)
		{
			if (Registration >= 0)
			{
				return false;
			}
			Result.Error = std::string(Operation)
				+ " failed for stable symbol " + Symbol.StableId
				+ " (AngelScript result "
				+ std::to_string(Registration) + ")";
			return true;
		}

		std::string TypedefTarget(const std::string_view Declaration)
		{
			const std::size_t Equals = Declaration.find('=');
			if (Equals != std::string_view::npos)
			{
				std::string Target(Declaration.substr(Equals + 1));
				const std::size_t Semicolon = Target.find(';');
				if (Semicolon != std::string::npos)
				{
					Target.resize(Semicolon);
				}
				while (!Target.empty() && Target.front() == ' ')
				{
					Target.erase(Target.begin());
				}
				while (!Target.empty() && Target.back() == ' ')
				{
					Target.pop_back();
				}
				return Target;
			}
			return {};
		}

		bool RegisterEarlyType(
			asIScriptEngine* Engine,
			const FOfflineSymbolRecord& Symbol,
			FRegistrationLoadResult& Result)
		{
			const FOfflineTypeRecord& Type = Symbol.Type;
			FNamespaceScope Namespace(Engine, Type.Namespace);
			if (Type.Kind == "primitive")
			{
				const int TypeId = Engine->GetTypeIdByDecl(Type.Name.c_str());
				if (TypeId < 0)
				{
					Result.Error =
						"bundle references an unknown primitive type: "
						+ Type.Name;
					return false;
				}
				Result.RuntimeMap.TypeIdByStableId.emplace(
					Symbol.StableId,
					TypeId);
				AttachOfflineTypeTraits(Engine, TypeId, Type);
				return true;
			}
			if (Type.Kind == "enum")
			{
				const int TypeId = Engine->RegisterEnum(Type.Name.c_str());
				if (FailRegistration(
						Result,
						Symbol,
						"enum registration",
						TypeId))
				{
					return false;
				}
				for (const FOfflineTypeRecord::FEnumValue& Value
					: Type.EnumValues)
				{
					if (Value.Value < std::numeric_limits<int>::min()
						|| Value.Value > std::numeric_limits<int>::max())
					{
						Result.Error =
							"enum value is outside the AngelScript integer range: "
							+ Value.Name;
						return false;
					}
					const int Registration = Engine->RegisterEnumValue(
						Type.Name.c_str(),
						Value.Name.c_str(),
						static_cast<int>(Value.Value));
					if (FailRegistration(
							Result,
							Symbol,
							"enum value registration",
							Registration))
					{
						return false;
					}
				}
				Result.RuntimeMap.TypeIdByStableId.emplace(
					Symbol.StableId,
					TypeId);
				AttachOfflineTypeTraits(Engine, TypeId, Type);
				return true;
			}
			if (Type.Kind == "typedef")
			{
				const std::string Target =
					TypedefTarget(Type.CompleteDeclaration);
				if (Target.empty())
				{
					Result.Error =
						"typedef record has no parseable target declaration: "
						+ Type.CompleteDeclaration;
					return false;
				}
				const int TypeId = Engine->RegisterTypedef(
					Type.Name.c_str(),
					Target.c_str());
				if (FailRegistration(
						Result,
						Symbol,
						"typedef registration",
						TypeId))
				{
					return false;
				}
				Result.RuntimeMap.TypeIdByStableId.emplace(
					Symbol.StableId,
					TypeId);
				return true;
			}
			if (Type.Kind == "funcdef" || Type.Kind == "delegate")
			{
				const int TypeId = Engine->RegisterFuncdef(
					Type.CompleteDeclaration.c_str());
				if (FailRegistration(
						Result,
						Symbol,
						"funcdef registration",
						TypeId))
				{
					return false;
				}
				Result.RuntimeMap.TypeIdByStableId.emplace(
					Symbol.StableId,
					TypeId);
				return true;
			}
			Result.Error = "unsupported early type kind: " + Type.Kind;
			return false;
		}

		bool RegisterTypeSkeleton(
			asIScriptEngine* Engine,
			const FOfflineSymbolRecord& Symbol,
			FRegistrationLoadResult& Result,
			std::map<std::string, FRegisteredTypeName>& OutNames)
		{
			const FOfflineTypeRecord& Type = Symbol.Type;
			FNamespaceScope Namespace(Engine, Type.Namespace);
			if (Type.bTemplateDefinition
				|| !Type.TemplateSubtypeDeclarations.empty()
				|| !Type.AdapterStableId.empty())
			{
				return RegisterAdapterTemplate(
					Engine,
					Symbol,
					Result,
					OutNames);
			}

			int ByteSize = 0;
			asDWORD Flags = 0;
			if (Type.Kind == "value")
			{
				if (Type.CompileSize > std::numeric_limits<int>::max())
				{
					Result.Error =
						"value type compile size exceeds host limits: "
						+ Type.Name;
					return false;
				}
				ByteSize = Type.CompileSize > 0
					? static_cast<int>(Type.CompileSize)
					: 1;
				Flags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS;
				if (Type.CompileSize <= 0)
				{
					Result.Capabilities.push_back({
						Symbol.StableId,
						"value-layout",
						ECapabilityClassification::CompileShim,
						"missing UE layout uses a one-byte validation-only value",
					});
				}
			}
			else
			{
				Flags =
					asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
			}
			const int TypeId = Engine->RegisterObjectType(
				Type.Name.c_str(),
				ByteSize,
				Flags);
			if (FailRegistration(
					Result,
					Symbol,
					"object type registration",
					TypeId))
			{
				return false;
			}
			Result.RuntimeMap.TypeIdByStableId.emplace(
				Symbol.StableId,
				TypeId);
			AttachOfflineTypeTraits(Engine, TypeId, Type);
			OutNames.emplace(
				Symbol.StableId,
				FRegisteredTypeName{
					Type.Namespace,
					Type.Name,
					Type.Kind != "value",
				});
			return true;
		}

		bool RegisterMemberOrGlobal(
			asIScriptEngine* Engine,
			const FOfflineSymbolRecord& Symbol,
			const std::map<std::string, FRegisteredTypeName>& TypeNames,
			const std::set<std::string>& ImplicitHandleTypes,
			const bool bFloatIsFloat64,
			FRegistrationLoadResult& Result)
		{
			if (!Symbol.Callable.StableId.empty())
			{
				const FOfflineCallableRecord& Callable = Symbol.Callable;
				const std::string RegistrationDeclaration =
					NormalizeRegistrationDeclaration(
						Callable.Declaration,
						ImplicitHandleTypes,
						bFloatIsFloat64,
						IsObjectWrapperAdapter(
							Callable.AdapterStableId));
				int Registration = -1;
				if (Callable.OwnerStableId.empty())
				{
					FNamespaceScope Namespace(Engine, Callable.Namespace);
					Registration = Engine->RegisterGlobalFunction(
						RegistrationDeclaration.c_str(),
						asFUNCTION(CompileOnlyTrap),
						asCALL_GENERIC);
				}
				else
				{
					const auto Owner = TypeNames.find(
						Callable.OwnerStableId);
					if (Owner == TypeNames.end())
					{
						Result.Error =
							"callable owner was not registered: "
							+ Callable.OwnerStableId;
						return false;
					}
					FNamespaceScope Namespace(
						Engine,
						Owner->second.Namespace);
					if (Callable.Kind == "method"
						|| Callable.Kind == "event"
						|| Callable.Kind == "global-function")
					{
						Registration = Engine->RegisterObjectMethod(
							Owner->second.Name.c_str(),
							RegistrationDeclaration.c_str(),
							asFUNCTION(CompileOnlyTrap),
							asCALL_GENERIC);
					}
					else if (Callable.Kind == "constructor"
						|| Callable.Kind == "destructor")
					{
						const std::size_t Parameters =
							RegistrationDeclaration.find('(');
						if (Parameters == std::string::npos)
						{
							Result.Error =
								"behavior record has no parameter list: "
								+ Callable.Declaration;
							return false;
						}
						const std::string BehaviorDeclaration =
							"void f"
							+ RegistrationDeclaration.substr(Parameters);
						Registration =
							Engine->RegisterObjectBehaviour(
								Owner->second.Name.c_str(),
								Callable.Kind == "constructor"
									? asBEHAVE_CONSTRUCT
									: asBEHAVE_DESTRUCT,
								BehaviorDeclaration.c_str(),
								asFUNCTION(CompileOnlyTrap),
								asCALL_GENERIC);
					}
					else
					{
						Result.Capabilities.push_back({
							Symbol.StableId,
							"object-behavior",
							ECapabilityClassification::CompileShim,
							"constructor/factory/destructor surface is not executable",
						});
						return true;
					}
				}
				if (FailRegistration(
						Result,
						Symbol,
						"callable registration",
						Registration))
				{
					return false;
				}
				Result.RuntimeMap.FunctionIdByStableId.emplace(
					Symbol.StableId,
					Registration);
				Result.Capabilities.push_back({
					Symbol.StableId,
					"callable",
					ECapabilityClassification::Exact,
					"declaration is registered with a non-executable generic trap",
				});
				return true;
			}

			const FOfflinePropertyRecord& Property = Symbol.Property;
			const std::string RegistrationDeclaration =
				NormalizeRegistrationDeclaration(
					Property.Declaration,
					ImplicitHandleTypes,
					bFloatIsFloat64);
			int Registration = -1;
			if (Property.OwnerStableId.empty())
			{
				Result.BackingStore->GlobalStorage.emplace_back();
				FNamespaceScope Namespace(Engine, Property.Namespace);
				Registration = Engine->RegisterGlobalProperty(
					RegistrationDeclaration.c_str(),
					Result.BackingStore->GlobalStorage.back().data());
			}
			else
			{
				const auto Owner = TypeNames.find(Property.OwnerStableId);
				if (Owner == TypeNames.end())
				{
					Result.Error =
						"property owner was not registered: "
						+ Property.OwnerStableId;
					return false;
				}
				FNamespaceScope Namespace(
					Engine,
					Owner->second.Namespace);
					Registration = Engine->RegisterObjectProperty(
					Owner->second.Name.c_str(),
					RegistrationDeclaration.c_str(),
					0);
			}
			if (FailRegistration(
					Result,
					Symbol,
					"property registration",
					Registration))
			{
				return false;
			}
			Result.RuntimeMap.PropertyIndexByStableId.emplace(
				Symbol.StableId,
				Registration);
			Result.Capabilities.push_back({
				Symbol.StableId,
				"property-layout",
				ECapabilityClassification::CompileShim,
				"property offsets/storage are validation-only",
			});
			return true;
		}

		bool RegisterTypeRelationships(
			asIScriptEngine* Engine,
			const FOfflineSymbolRecord& Symbol,
			const std::map<std::string, FRegisteredTypeName>& TypeNames,
			const std::map<
				std::string,
				const FOfflineSymbolRecord*>& TypeSymbols,
			FRegistrationLoadResult& Result)
		{
			const auto Owner = TypeNames.find(Symbol.StableId);
			if (Owner == TypeNames.end())
			{
				Result.Error =
					"type relationship owner was not registered: "
					+ Symbol.StableId;
				return false;
			}

			std::vector<std::string> Pending =
				Symbol.Type.InterfaceStableIds;
			if (!Symbol.Type.BaseStableId.empty())
			{
				Pending.push_back(Symbol.Type.BaseStableId);
			}
			std::set<std::string> TargetSet;
			while (!Pending.empty())
			{
				std::string TargetStableId =
					std::move(Pending.back());
				Pending.pop_back();
				if (TargetStableId == Symbol.StableId
					|| !TargetSet.insert(TargetStableId).second)
				{
					continue;
				}
				const auto TargetSymbol =
					TypeSymbols.find(TargetStableId);
				if (TargetSymbol == TypeSymbols.end())
				{
					Result.Error =
						"type relationship target record is missing: "
						+ TargetStableId;
					return false;
				}
				if (!TargetSymbol->second->Type.BaseStableId.empty())
				{
					Pending.push_back(
						TargetSymbol->second->Type.BaseStableId);
				}
				Pending.insert(
					Pending.end(),
					TargetSymbol->second->Type.InterfaceStableIds.begin(),
					TargetSymbol->second->Type.InterfaceStableIds.end());
			}
			const std::vector<std::string> Targets(
				TargetSet.begin(),
				TargetSet.end());

			for (const std::string& TargetStableId : Targets)
			{
				const auto Target = TypeNames.find(TargetStableId);
				if (Target == TypeNames.end())
				{
					Result.Error =
						"type relationship target was not registered: "
						+ TargetStableId;
					return false;
				}

				std::string TargetDeclaration;
				if (!Target->second.Namespace.empty()
					&& Target->second.Namespace
						!= Owner->second.Namespace)
				{
					TargetDeclaration =
						"::" + Target->second.Namespace + "::";
				}
				TargetDeclaration += Target->second.Name;
				const std::string Declaration =
					TargetDeclaration + " opImplCast() const";

				FNamespaceScope Namespace(
					Engine,
					Owner->second.Namespace);
				const int Registration = Engine->RegisterObjectMethod(
					Owner->second.Name.c_str(),
					Declaration.c_str(),
					asFUNCTION(CompileOnlyTrap),
					asCALL_GENERIC);
				if (Registration < 0)
				{
					Result.Error =
						"type relationship cast registration failed for "
						+ Symbol.StableId + " -> " + TargetStableId
						+ " (AngelScript result "
						+ std::to_string(Registration) + ")";
					return false;
				}
				Result.Capabilities.push_back({
					Symbol.StableId,
					"type-relationship:"
						+ Symbol.StableId + ":" + TargetStableId,
					ECapabilityClassification::CompileShim,
					"UE base/interface conversion is represented by a "
						"non-executable implicit cast",
				});
			}
			return true;
		}
	}

	std::string NormalizeApplicationRegistrationDeclaration(
		const std::string_view Declaration,
		const FOfflineManifest& Manifest)
	{
		const auto FloatWidth = Manifest.EngineProperties.find(
			"angelscript.float-width");
		const bool bFloatIsFloat64 =
			FloatWidth == Manifest.EngineProperties.end()
				|| FloatWidth->second == "64";
		return NormalizeRegistrationDeclaration(
			Declaration,
			{},
			bFloatIsFloat64);
	}

	FRegistrationLoadResult ApplyRegistrationPlan(
		asIScriptEngine* Engine,
		const FRegistrationPlan& Plan,
		const FOfflineManifest& Manifest)
	{
		FRegistrationLoadResult Result;
		Result.BackingStore =
			std::make_shared<FRegistrationBackingStore>();
		if (Engine == nullptr)
		{
			Result.Error = "cannot apply a registration plan to a null engine";
			return Result;
		}
		if (!Plan.bSuccess)
		{
			Result.Error = "cannot apply an invalid registration plan";
			return Result;
		}
		const auto FloatWidth =
			Manifest.EngineProperties.find(
				"angelscript.float-width");
		const bool bFloatIsFloat64 =
			FloatWidth == Manifest.EngineProperties.end()
				|| FloatWidth->second == "64";
		if (FloatWidth != Manifest.EngineProperties.end()
			&& FloatWidth->second != "32"
			&& FloatWidth->second != "64")
		{
			Result.Error =
				"unsupported angelscript.float-width engine property: "
				+ FloatWidth->second;
			return Result;
		}
		if (Engine->SetEngineProperty(
				asEP_ALLOW_IMPLICIT_HANDLE_TYPES,
				1) < 0
			|| Engine->SetEngineProperty(
				asEP_ALLOW_UNSAFE_REFERENCES,
				1) < 0
			|| Engine->SetEngineProperty(
				asEP_FLOAT_IS_FLOAT64,
				bFloatIsFloat64 ? 1 : 0) < 0
			|| Engine->SetEngineProperty(
				asEP_ALTER_SYNTAX_NAMED_ARGS,
				1) < 0)
		{
			Result.Error =
				"failed to configure UE-validation engine properties";
			return Result;
		}

		std::map<std::string, FRegisteredTypeName> TypeNames;
		std::map<std::string, const FOfflineSymbolRecord*> TypeSymbols;
		for (const FRegistrationPlanItem& Item : Plan.Items)
		{
			if (Item.Symbol != nullptr)
			{
				Result.RuntimeMap.AvailabilityByStableId.emplace(
					Item.Symbol->StableId,
					GetAvailability(*Item.Symbol));
			}
			if (Item.Symbol != nullptr
				&& !Item.Symbol->Type.StableId.empty())
			{
				TypeSymbols.emplace(
					Item.Symbol->StableId,
					Item.Symbol);
			}
		}
		std::set<std::string> ImplicitHandleTypes;
		for (const FRegistrationPlanItem& Item : Plan.Items)
		{
			if (Item.Stage == ERegistrationStage::EngineSettings
				|| Item.Stage == ERegistrationStage::Adapters
				|| Item.Stage == ERegistrationStage::Sources)
			{
				continue;
			}
			if (Item.Symbol == nullptr)
			{
				Result.Error =
					"registration plan contains an empty symbol item";
				return Result;
			}
			switch (Item.Stage)
			{
			case ERegistrationStage::EnumTypedefFuncdef:
				if (!RegisterEarlyType(Engine, *Item.Symbol, Result))
					return Result;
				break;
			case ERegistrationStage::TypeSkeleton:
				if (!RegisterTypeSkeleton(
						Engine,
						*Item.Symbol,
						Result,
						TypeNames))
				{
					return Result;
				}
				if (const auto Registered =
						TypeNames.find(Item.Symbol->StableId);
					Registered != TypeNames.end()
						&& Registered->second.bImplicitHandle)
				{
					ImplicitHandleTypes.insert(
						Registered->second.Name);
					if (!Registered->second.Namespace.empty())
					{
						ImplicitHandleTypes.insert(
							Registered->second.Namespace
							+ "::" + Registered->second.Name);
					}
				}
				break;
			case ERegistrationStage::TypeRelationships:
				if ((!Item.Symbol->Type.BaseStableId.empty()
						|| !Item.Symbol->Type.InterfaceStableIds.empty())
					&& !RegisterTypeRelationships(
						Engine,
						*Item.Symbol,
						TypeNames,
						TypeSymbols,
						Result))
				{
					return Result;
				}
				break;
			case ERegistrationStage::MembersAndGlobals:
				if (!RegisterMemberOrGlobal(
						Engine,
						*Item.Symbol,
						TypeNames,
						ImplicitHandleTypes,
						bFloatIsFloat64,
						Result))
				{
					return Result;
				}
				break;
			default:
				break;
			}
		}
		const FOfflineSymbolRecord* StringTypeSymbol = nullptr;
		for (const FRegistrationPlanItem& Item : Plan.Items)
		{
			if (Item.Symbol != nullptr
				&& Item.Symbol->Kind == "type"
				&& Item.Symbol->Type.Name == "FString"
				&& Item.Symbol->Type.Namespace.empty())
			{
				StringTypeSymbol = Item.Symbol;
				break;
			}
		}
		if (StringTypeSymbol != nullptr)
		{
			auto StringFactory =
				std::make_unique<FCompileOnlyStringFactory>(
					static_cast<std::size_t>(
						StringTypeSymbol->Type.CompileSize));
			const int Registration = Engine->RegisterStringFactory(
				"FString",
				StringFactory.get());
			if (Registration < 0)
			{
				Result.Error =
					"FString compile-only string factory registration failed "
					"for stable symbol "
					+ StringTypeSymbol->StableId
					+ " (AngelScript result "
					+ std::to_string(Registration) + ")";
				return Result;
			}
			Result.BackingStore->StringFactory =
				std::move(StringFactory);
			Result.Capabilities.push_back({
				StringTypeSymbol->StableId,
				"string-literals",
				ECapabilityClassification::CompileShim,
				"FString literals use validation-only backing storage",
			});
		}
		Result.bSuccess = true;
		return Result;
	}
}

#include "AngelscriptOfflineContractIdentity.h"

#include "HAL/UnrealMemory.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		constexpr uint32 Sha256RoundConstants[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
			0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
			0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
			0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
			0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
			0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
			0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
			0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
			0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
		};

		uint32 RotateRight(const uint32 Value, const uint32 Shift)
		{
			return (Value >> Shift) | (Value << (32u - Shift));
		}

		uint32 ReadBigEndian(const uint8* Data)
		{
			return (static_cast<uint32>(Data[0]) << 24)
				| (static_cast<uint32>(Data[1]) << 16)
				| (static_cast<uint32>(Data[2]) << 8)
				| static_cast<uint32>(Data[3]);
		}

		void TransformSha256(uint32 (&State)[8], const uint8* Block)
		{
			uint32 Words[64] = {};
			for (uint32 Index = 0; Index < 16; ++Index)
			{
				Words[Index] = ReadBigEndian(Block + Index * 4u);
			}
			for (uint32 Index = 16; Index < 64; ++Index)
			{
				const uint32 S0 = RotateRight(Words[Index - 15], 7)
					^ RotateRight(Words[Index - 15], 18)
					^ (Words[Index - 15] >> 3);
				const uint32 S1 = RotateRight(Words[Index - 2], 17)
					^ RotateRight(Words[Index - 2], 19)
					^ (Words[Index - 2] >> 10);
				Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
			}

			uint32 A = State[0];
			uint32 B = State[1];
			uint32 C = State[2];
			uint32 D = State[3];
			uint32 E = State[4];
			uint32 F = State[5];
			uint32 G = State[6];
			uint32 H = State[7];
			for (uint32 Index = 0; Index < 64; ++Index)
			{
				const uint32 Sum1 = RotateRight(E, 6)
					^ RotateRight(E, 11)
					^ RotateRight(E, 25);
				const uint32 Choice = (E & F) ^ (~E & G);
				const uint32 Temporary1 =
					H + Sum1 + Choice + Sha256RoundConstants[Index] + Words[Index];
				const uint32 Sum0 = RotateRight(A, 2)
					^ RotateRight(A, 13)
					^ RotateRight(A, 22);
				const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
				const uint32 Temporary2 = Sum0 + Majority;
				H = G;
				G = F;
				F = E;
				E = D + Temporary1;
				D = C;
				C = B;
				B = A;
				A = Temporary1 + Temporary2;
			}

			State[0] += A;
			State[1] += B;
			State[2] += C;
			State[3] += D;
			State[4] += E;
			State[5] += F;
			State[6] += G;
			State[7] += H;
		}

		FString ComputeSha256(const void* Data, const uint32 ByteSize)
		{
			if (!ensureMsgf(
				ByteSize == 0 || Data != nullptr,
				TEXT("SHA-256 input must be non-null when ByteSize is non-zero")))
			{
				return FString();
			}

			uint32 State[8] = {
				0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
				0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
			};
			const uint8* Bytes = static_cast<const uint8*>(Data);
			const uint32 FullBlockBytes = ByteSize - (ByteSize % 64u);
			for (uint32 Offset = 0; Offset < FullBlockBytes; Offset += 64u)
			{
				TransformSha256(State, Bytes + Offset);
			}

			uint8 FinalBlocks[128] = {};
			const uint32 RemainingBytes = ByteSize - FullBlockBytes;
			if (RemainingBytes > 0)
			{
				FMemory::Memcpy(FinalBlocks, Bytes + FullBlockBytes, RemainingBytes);
			}
			FinalBlocks[RemainingBytes] = 0x80u;
			const uint32 FinalBlockCount = RemainingBytes < 56u ? 1u : 2u;
			const uint64 BitLength = static_cast<uint64>(ByteSize) * 8u;
			for (uint32 Index = 0; Index < 8; ++Index)
			{
				FinalBlocks[FinalBlockCount * 64u - 1u - Index] =
					static_cast<uint8>((BitLength >> (Index * 8u)) & 0xffu);
			}
			for (uint32 Index = 0; Index < FinalBlockCount; ++Index)
			{
				TransformSha256(State, FinalBlocks + Index * 64u);
			}

			FString Result;
			Result.Reserve(64);
			for (const uint32 Word : State)
			{
				Result += FString::Printf(TEXT("%08x"), Word);
			}
			return Result;
		}

		bool IsDeclarationNoSpaceBefore(const TCHAR Character)
		{
			switch (Character)
			{
			case TEXT('('):
			case TEXT(')'):
			case TEXT('['):
			case TEXT(']'):
			case TEXT('{'):
			case TEXT('}'):
			case TEXT(','):
			case TEXT(';'):
			case TEXT('&'):
			case TEXT('*'):
			case TEXT('@'):
			case TEXT('<'):
			case TEXT('>'):
			case TEXT('='):
				return true;
			default:
				return false;
			}
		}

		bool IsDeclarationNoSpaceAfter(const TCHAR Character)
		{
			switch (Character)
			{
			case TEXT('('):
			case TEXT('['):
			case TEXT('{'):
			case TEXT(','):
			case TEXT('<'):
			case TEXT('='):
				return true;
			default:
				return false;
			}
		}

		void RemoveTrailingWhitespace(FString& Value)
		{
			while (!Value.IsEmpty() && FChar::IsWhitespace(Value[Value.Len() - 1]))
			{
				Value.LeftChopInline(1, EAllowShrinking::No);
			}
		}
	}

	FString NormalizeNamespace(const FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsWhitespace(Character))
			{
				Result.AppendChar(Character);
			}
		}
		return Result;
	}

	FString NormalizeDeclaration(const FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len());

		bool bPendingWhitespace = false;
		bool bInString = false;
		bool bInCharacter = false;
		bool bEscaped = false;

		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			if (bInString || bInCharacter)
			{
				Result.AppendChar(Character);
				if (bEscaped)
				{
					bEscaped = false;
				}
				else if (Character == TEXT('\\'))
				{
					bEscaped = true;
				}
				else if ((bInString && Character == TEXT('"'))
					|| (bInCharacter && Character == TEXT('\'')))
				{
					bInString = false;
					bInCharacter = false;
				}
				continue;
			}

			if (FChar::IsWhitespace(Character))
			{
				bPendingWhitespace = !Result.IsEmpty();
				continue;
			}

			if (Character == TEXT('"') || Character == TEXT('\''))
			{
				if (bPendingWhitespace
					&& !Result.IsEmpty()
					&& !IsDeclarationNoSpaceAfter(Result[Result.Len() - 1]))
				{
					Result.AppendChar(TEXT(' '));
				}
				Result.AppendChar(Character);
				bPendingWhitespace = false;
				bInString = Character == TEXT('"');
				bInCharacter = Character == TEXT('\'');
				continue;
			}

			if (Character == TEXT(':')
				&& Index + 1 < Value.Len()
				&& Value[Index + 1] == TEXT(':'))
			{
				RemoveTrailingWhitespace(Result);
				Result.Append(TEXT("::"));
				++Index;
				bPendingWhitespace = false;
				continue;
			}

			if (IsDeclarationNoSpaceBefore(Character))
			{
				RemoveTrailingWhitespace(Result);
				Result.AppendChar(Character);
				bPendingWhitespace = false;
				continue;
			}

			if (bPendingWhitespace
				&& !Result.IsEmpty()
				&& !IsDeclarationNoSpaceAfter(Result[Result.Len() - 1]))
			{
				Result.AppendChar(TEXT(' '));
			}
			Result.AppendChar(Character);
			bPendingWhitespace = false;
		}

		RemoveTrailingWhitespace(Result);
		return Result;
	}

	FString NormalizeSemanticPath(const FStringView Value)
	{
		FString Result(Value);
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

		while (Result.Contains(TEXT("//"), ESearchCase::CaseSensitive))
		{
			Result.ReplaceInline(TEXT("//"), TEXT("/"), ESearchCase::CaseSensitive);
		}

		while (Result.StartsWith(TEXT("./"), ESearchCase::CaseSensitive))
		{
			Result.RightChopInline(2, EAllowShrinking::No);
		}

		while (Result.EndsWith(TEXT("/"), ESearchCase::CaseSensitive) && Result.Len() > 1)
		{
			Result.LeftChopInline(1, EAllowShrinking::No);
		}
		return Result;
	}

	FString Sha256Utf8(const FStringView Value)
	{
		const FString Owned(Value);
		const FTCHARToUTF8 Utf8(*Owned, Owned.Len());
		return ComputeSha256(
			Utf8.Get(),
			static_cast<uint32>(Utf8.Length()));
	}

	FString Sha256Bytes(const TArray<uint8>& Value)
	{
		return ComputeSha256(
			Value.GetData(),
			static_cast<uint32>(Value.Num()));
	}

	FString MakeCanonicalSymbolIdentity(const FSymbolIdentityInput& Input)
	{
		return FString::Printf(
			TEXT("%s\n%s\n%s\n%s\n%s"),
			SymbolIdentityVersion,
			LexToString(Input.Kind),
			*NormalizeNamespace(Input.Namespace),
			*Input.OwnerStableId.TrimStartAndEnd(),
			*NormalizeDeclaration(Input.CompleteDeclaration));
	}

	FString MakeStableSymbolId(const FSymbolIdentityInput& Input)
	{
		return Sha256Utf8(MakeCanonicalSymbolIdentity(Input));
	}

	FString MakeStableModuleId(
		const FStringView LogicalModuleName,
		const FStringView VirtualSourceIdentity)
	{
		const FString Identity = FString::Printf(
			TEXT("%s\n%s\n%s"),
			ModuleIdentityVersion,
			*NormalizeSemanticPath(LogicalModuleName),
			*NormalizeSemanticPath(VirtualSourceIdentity));
		return Sha256Utf8(Identity);
	}

	FString MakeStableAdapterId(
		const FStringView AdapterName,
		const FStringView AdapterVersion)
	{
		const FString Identity = FString::Printf(
			TEXT("%s\n%s\n%s"),
			AdapterIdentityVersion,
			*NormalizeNamespace(AdapterName),
			*FString(AdapterVersion).TrimStartAndEnd());
		return Sha256Utf8(Identity);
	}

	FString MakeStableAssetId(const FStringView NormalizedObjectPath)
	{
		const FString Identity = FString::Printf(
			TEXT("%s\n%s"),
			AssetIdentityVersion,
			*NormalizeSemanticPath(NormalizedObjectPath));
		return Sha256Utf8(Identity);
	}
}

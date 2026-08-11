#include "Cache/AngelscriptCacheStore.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace AngelscriptCacheAtomicFileOps_Private
{
	static FAngelscriptCacheStoreResult Failure(
		const EAngelscriptCacheStoreError Error,
		const uint32 PlatformError = 0)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			Error, EAngelscriptCacheStoreStage::None);
		if (PlatformError != 0)
		{
			Result.PlatformErrorCode = static_cast<int64>(PlatformError);
		}
		return Result;
	}

	static bool IsSameDirectory(const FString& First, const FString& Second)
	{
		return FPaths::GetPath(First).Equals(
			FPaths::GetPath(Second), ESearchCase::IgnoreCase);
	}

#if PLATFORM_WINDOWS
	class FWindowsAngelscriptCachePinnedFileHandle final
		: public IAngelscriptCachePinnedFileHandle
	{
	public:
		FWindowsAngelscriptCachePinnedFileHandle(
			const HANDLE InHandle,
			const uint64 InSize)
			: Handle(InHandle)
			, Size(InSize)
		{
			check(Handle != INVALID_HANDLE_VALUE);
			check(Size <= static_cast<uint64>(MAX_int32));
		}

		virtual ~FWindowsAngelscriptCachePinnedFileHandle() override
		{
			if (Handle != INVALID_HANDLE_VALUE)
			{
				::CloseHandle(Handle);
				Handle = INVALID_HANDLE_VALUE;
			}
		}

		virtual uint64 GetSize() const override
		{
			return Size;
		}

		virtual FAngelscriptCacheStoreResult ReadAll(
			TArray<uint8>& OutBytes) override
		{
			OutBytes.Reset();
			LARGE_INTEGER Start{};
			if (!::SetFilePointerEx(Handle, Start, nullptr, FILE_BEGIN))
			{
				return Failure(
					EAngelscriptCacheStoreError::ReadFailed,
					::GetLastError());
			}

			OutBytes.SetNumUninitialized(static_cast<int32>(Size));
			uint64 Offset = 0;
			while (Offset < Size)
			{
				const DWORD Chunk = static_cast<DWORD>(FMath::Min<uint64>(
					Size - Offset, MAXDWORD));
				DWORD Read = 0;
				if (!::ReadFile(
						Handle,
						OutBytes.GetData() + static_cast<int32>(Offset),
						Chunk,
						&Read,
						nullptr)
					|| Read != Chunk)
				{
					const uint32 Error = ::GetLastError();
					OutBytes.Reset();
					return Failure(EAngelscriptCacheStoreError::ReadFailed, Error);
				}
				Offset += Read;
			}
			return FAngelscriptCacheStoreResult::Success();
		}

	private:
		HANDLE Handle = INVALID_HANDLE_VALUE;
		uint64 Size = 0;
	};

	static FString ToWindowsNativePath(const FString& Path)
	{
		FString Native(Path);
		Native.ReplaceInline(TEXT("/"), TEXT("\\"));
		return Native;
	}

	static EAngelscriptCacheStoreError ClassifyMissingRead(const FString& Path)
	{
		if (Path.EndsWith(TEXT(".aspack"), ESearchCase::CaseSensitive))
		{
			return EAngelscriptCacheStoreError::PackMissing;
		}
		if (Path.EndsWith(TEXT(".asmanifest"), ESearchCase::CaseSensitive))
		{
			return EAngelscriptCacheStoreError::ManifestMissing;
		}
		return EAngelscriptCacheStoreError::PointerInvalid;
	}

	static bool IsNotFoundError(const uint32 Error)
	{
		return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND;
	}

	static bool ResolveExistingDirectory(
		const FString& ExistingDirectory,
		FString& OutResolved)
	{
		const FString Native = ToWindowsNativePath(ExistingDirectory);
		const HANDLE Handle = ::CreateFileW(
			*Native,
			0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS,
			nullptr);
		if (Handle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		const DWORD Required = ::GetFinalPathNameByHandleW(
			Handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (Required == 0)
		{
			::CloseHandle(Handle);
			return false;
		}

		TArray<WCHAR> Buffer;
		Buffer.SetNumUninitialized(static_cast<int32>(Required + 1));
		const DWORD Written = ::GetFinalPathNameByHandleW(
			Handle,
			Buffer.GetData(),
			static_cast<DWORD>(Buffer.Num()),
			FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		::CloseHandle(Handle);
		if (Written == 0 || Written >= static_cast<DWORD>(Buffer.Num()))
		{
			return false;
		}

		OutResolved = FString(static_cast<int32>(Written), Buffer.GetData());
		if (OutResolved.StartsWith(TEXT("\\\\?\\UNC\\"), ESearchCase::CaseSensitive))
		{
			OutResolved = TEXT("//") + OutResolved.Mid(8);
		}
		else if (OutResolved.StartsWith(TEXT("\\\\?\\"), ESearchCase::CaseSensitive))
		{
			OutResolved.RightChopInline(4, EAllowShrinking::No);
		}
		FPaths::NormalizeDirectoryName(OutResolved);
		return !OutResolved.IsEmpty() && !FPaths::IsRelative(OutResolved);
	}

	static bool IsFixedLocalVolume(const FString& AbsolutePath)
	{
		const FString Native = ToWindowsNativePath(AbsolutePath);
		WCHAR VolumePath[MAX_PATH]{};
		if (!::GetVolumePathNameW(*Native, VolumePath, UE_ARRAY_COUNT(VolumePath)))
		{
			return false;
		}
		return ::GetDriveTypeW(VolumePath) == DRIVE_FIXED;
	}

	class FWindowsAngelscriptCacheAtomicFileOps final
		: public IAngelscriptCacheAtomicFileOps
	{
	public:
		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString& RequestedBaseRoot,
			FAngelscriptCanonicalCacheRoot& OutRoot) override
		{
			OutRoot = FAngelscriptCanonicalCacheRoot{};
			if (RequestedBaseRoot.IsEmpty() || FPaths::IsRelative(RequestedBaseRoot))
			{
				return Failure(EAngelscriptCacheStoreError::InvalidRoot);
			}

			FString Normalized = FPaths::ConvertRelativePathToFull(RequestedBaseRoot);
			FPaths::NormalizeDirectoryName(Normalized);
			if (!FPaths::CollapseRelativeDirectories(Normalized, true)
				|| Normalized.IsEmpty() || FPaths::IsRelative(Normalized))
			{
				return Failure(EAngelscriptCacheStoreError::InvalidRoot);
			}

			TArray<FString> MissingComponents;
			FString Existing = Normalized;
			while (!IFileManager::Get().DirectoryExists(*Existing))
			{
				if (IFileManager::Get().FileExists(*Existing))
				{
					return Failure(EAngelscriptCacheStoreError::InvalidRoot);
				}
				const FString Parent = FPaths::GetPath(Existing);
				const FString Leaf = FPaths::GetCleanFilename(Existing);
				if (Parent.IsEmpty() || Parent == Existing || Leaf.IsEmpty())
				{
					return Failure(EAngelscriptCacheStoreError::InvalidRoot);
				}
				MissingComponents.Add(Leaf);
				Existing = Parent;
			}

			FString Resolved;
			if (!ResolveExistingDirectory(Existing, Resolved))
			{
				return Failure(EAngelscriptCacheStoreError::InvalidRoot, ::GetLastError());
			}
			for (int32 Index = MissingComponents.Num() - 1; Index >= 0; --Index)
			{
				Resolved = FPaths::Combine(Resolved, MissingComponents[Index]);
			}
			FPaths::NormalizeDirectoryName(Resolved);
			if (!IsFixedLocalVolume(Resolved))
			{
				return Failure(EAngelscriptCacheStoreError::InvalidRoot, ::GetLastError());
			}

			OutRoot.AbsolutePath = MoveTemp(Resolved);
			OutRoot.IdentityPath = OutRoot.AbsolutePath.ToLower();
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString& DirectoryPath) override
		{
			if (IFileManager::Get().DirectoryExists(*DirectoryPath)
				|| IFileManager::Get().MakeDirectory(*DirectoryPath, true))
			{
				return FAngelscriptCacheStoreResult::Success();
			}
			return Failure(EAngelscriptCacheStoreError::WriteFailed, ::GetLastError());
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString& TempPath,
			const TConstArrayView<uint8> Bytes) override
		{
			const FString Native = ToWindowsNativePath(TempPath);
			const HANDLE Handle = ::CreateFileW(
				*Native,
				GENERIC_WRITE,
				FILE_SHARE_READ,
				nullptr,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				nullptr);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				return Failure(EAngelscriptCacheStoreError::OpenFailed, ::GetLastError());
			}

			int64 Offset = 0;
			while (Offset < Bytes.Num())
			{
				const DWORD Chunk = static_cast<DWORD>(FMath::Min<int64>(
					Bytes.Num() - Offset, MAXDWORD));
				DWORD Written = 0;
				if (!::WriteFile(Handle, Bytes.GetData() + Offset, Chunk, &Written, nullptr)
					|| Written != Chunk)
				{
					const uint32 Error = ::GetLastError();
					::CloseHandle(Handle);
					return Failure(EAngelscriptCacheStoreError::WriteFailed, Error);
				}
				Offset += Written;
			}

			if (!::FlushFileBuffers(Handle))
			{
				const uint32 Error = ::GetLastError();
				::CloseHandle(Handle);
				return Failure(EAngelscriptCacheStoreError::FlushFailed, Error);
			}
			if (!::CloseHandle(Handle))
			{
				return Failure(EAngelscriptCacheStoreError::FlushFailed, ::GetLastError());
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			const uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			OutBytes.Reset();
			const FString Native = ToWindowsNativePath(Path);
			const HANDLE Handle = ::CreateFileW(
				*Native,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				const uint32 Error = ::GetLastError();
				return Failure(
					IsNotFoundError(Error)
						? ClassifyMissingRead(Path)
						: EAngelscriptCacheStoreError::OpenFailed,
					Error);
			}

			LARGE_INTEGER Size{};
			if (!::GetFileSizeEx(Handle, &Size) || Size.QuadPart < 0
				|| static_cast<uint64>(Size.QuadPart) > MaxBytes
				|| Size.QuadPart > MAX_int32)
			{
				const uint32 Error = ::GetLastError();
				::CloseHandle(Handle);
				return Failure(EAngelscriptCacheStoreError::ReadFailed, Error);
			}

			OutBytes.SetNumUninitialized(static_cast<int32>(Size.QuadPart));
			int64 Offset = 0;
			while (Offset < Size.QuadPart)
			{
				const DWORD Chunk = static_cast<DWORD>(FMath::Min<int64>(
					Size.QuadPart - Offset, MAXDWORD));
				DWORD Read = 0;
				if (!::ReadFile(Handle, OutBytes.GetData() + Offset, Chunk, &Read, nullptr)
					|| Read != Chunk)
				{
					const uint32 Error = ::GetLastError();
					::CloseHandle(Handle);
					OutBytes.Reset();
					return Failure(EAngelscriptCacheStoreError::ReadFailed, Error);
				}
				Offset += Read;
			}
			if (!::CloseHandle(Handle))
			{
				OutBytes.Reset();
				return Failure(EAngelscriptCacheStoreError::ReadFailed, ::GetLastError());
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult OpenReadPinned(
			const FString& Path,
			const uint64 MaxBytes,
			TUniquePtr<IAngelscriptCachePinnedFileHandle>& OutHandle) override
		{
			OutHandle.Reset();
			const FString Native = ToWindowsNativePath(Path);
			const HANDLE Handle = ::CreateFileW(
				*Native,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				const uint32 Error = ::GetLastError();
				return Failure(
					IsNotFoundError(Error)
						? ClassifyMissingRead(Path)
						: EAngelscriptCacheStoreError::OpenFailed,
					Error);
			}

			LARGE_INTEGER Size{};
			if (!::GetFileSizeEx(Handle, &Size)
				|| Size.QuadPart < 0
				|| static_cast<uint64>(Size.QuadPart) > MaxBytes
				|| Size.QuadPart > MAX_int32)
			{
				const uint32 Error = ::GetLastError();
				::CloseHandle(Handle);
				return Failure(EAngelscriptCacheStoreError::ReadFailed, Error);
			}

			OutHandle = MakeUnique<FWindowsAngelscriptCachePinnedFileHandle>(
				Handle, static_cast<uint64>(Size.QuadPart));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
			const FString& DirectoryPath,
			TArray<FString>& OutFileNames) override
		{
			OutFileNames.Reset();
			FString NativePattern = ToWindowsNativePath(DirectoryPath);
			if (!NativePattern.EndsWith(TEXT("\\")))
			{
				NativePattern.AppendChar(TEXT('\\'));
			}
			NativePattern.AppendChar(TEXT('*'));

			WIN32_FIND_DATAW FindData{};
			const HANDLE FindHandle = ::FindFirstFileW(*NativePattern, &FindData);
			if (FindHandle == INVALID_HANDLE_VALUE)
			{
				return Failure(EAngelscriptCacheStoreError::ReadFailed, ::GetLastError());
			}

			for (;;)
			{
				if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					OutFileNames.Add(FindData.cFileName);
				}
				if (::FindNextFileW(FindHandle, &FindData))
				{
					continue;
				}
				const uint32 Error = ::GetLastError();
				::FindClose(FindHandle);
				if (Error != ERROR_NO_MORE_FILES)
				{
					OutFileNames.Reset();
					return Failure(EAngelscriptCacheStoreError::ReadFailed, Error);
				}
				return FAngelscriptCacheStoreResult::Success();
			}
		}

		virtual FAngelscriptCacheStoreResult RenameNewImmutable(
			const FString& TempPath,
			const FString& FinalPath) override
		{
			if (!IsSameDirectory(TempPath, FinalPath))
			{
				return Failure(EAngelscriptCacheStoreError::PathEscapesRoot);
			}
			if (::MoveFileExW(
				*ToWindowsNativePath(TempPath),
				*ToWindowsNativePath(FinalPath),
				MOVEFILE_WRITE_THROUGH))
			{
				return FAngelscriptCacheStoreResult::Success();
			}
			const uint32 Error = ::GetLastError();
			return Failure(
				Error == ERROR_ALREADY_EXISTS || Error == ERROR_FILE_EXISTS
					? EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption
					: EAngelscriptCacheStoreError::RenameFailed,
				Error);
		}

		virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
			const FString& TempPath,
			const FString& PointerPath) override
		{
			if (!IsSameDirectory(TempPath, PointerPath))
			{
				return Failure(EAngelscriptCacheStoreError::PathEscapesRoot);
			}
			if (!::MoveFileExW(
				*ToWindowsNativePath(TempPath),
				*ToWindowsNativePath(PointerPath),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				return Failure(EAngelscriptCacheStoreError::AtomicReplaceFailed, ::GetLastError());
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString& PointerPath) override
		{
			if (::DeleteFileW(*ToWindowsNativePath(PointerPath)))
			{
				return FAngelscriptCacheStoreResult::Success();
			}
			const uint32 Error = ::GetLastError();
			return IsNotFoundError(Error)
				? FAngelscriptCacheStoreResult::Success()
				: Failure(EAngelscriptCacheStoreError::PointerRemoveFailed, Error);
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString& TempPath) override
		{
			if (::DeleteFileW(*ToWindowsNativePath(TempPath)))
			{
				return FAngelscriptCacheStoreResult::Success();
			}
			const uint32 Error = ::GetLastError();
			return IsNotFoundError(Error)
				? FAngelscriptCacheStoreResult::Success()
				: Failure(EAngelscriptCacheStoreError::WriteFailed, Error);
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString& FinalPath) override
		{
			if (::DeleteFileW(*ToWindowsNativePath(FinalPath)))
			{
				return FAngelscriptCacheStoreResult::Success();
			}
			const uint32 Error = ::GetLastError();
			return IsNotFoundError(Error)
				? FAngelscriptCacheStoreResult::Success()
				: Failure(EAngelscriptCacheStoreError::DeleteDeferred, Error);
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString& DirectoryPath) override
		{
			if (!IFileManager::Get().DirectoryExists(*DirectoryPath))
			{
				return Failure(EAngelscriptCacheStoreError::DirectorySyncFailed);
			}
			// Win32 does not expose a generally usable directory fsync. Every Store
			// file is fully flushed before publication and both immutable and pointer
			// renames use MOVEFILE_WRITE_THROUGH, which is the documented Windows
			// durability boundary for the directory-entry change.
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}
	};
#endif
}

TUniquePtr<IAngelscriptCacheAtomicFileOps> CreateAngelscriptCacheAtomicFileOps()
{
#if PLATFORM_WINDOWS
	return MakeUnique<
		AngelscriptCacheAtomicFileOps_Private::FWindowsAngelscriptCacheAtomicFileOps>();
#else
	return nullptr;
#endif
}

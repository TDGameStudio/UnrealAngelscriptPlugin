#pragma once

#include "Cache/AngelscriptCacheStore.h"

// Rewrites one existing logical slot without Current/Previous rotation. The
// caller owns the namespace lock. OutSelected is true exactly when reread proves
// the new pointer is selected, including an indeterminate replace result.
FAngelscriptCacheStoreResult RewriteAngelscriptCachePointerForCompactionUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	EAngelscriptCachePointerKind Kind,
	const FAngelscriptHash256& NewGenerationId,
	const FAngelscriptCacheWriterToken& WriterToken,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	bool& OutSelected);

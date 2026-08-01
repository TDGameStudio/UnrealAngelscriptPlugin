#pragma once

struct UAngelscriptSettings
{
	bool bErrorOnIncorrectEditorOnlyCode = true;
	bool bWarnOnDivergentComparisonOperatorOverloads = true;
	bool bWarnOnImplicitSignedUnsignedConversion = true;
	bool bWarnOnIncrementDecrementInComplexExpression = true;
	bool bWarnOnUnusedReturnValueForConstMethods = true;
	bool bErrorWhenUsingInvalidWorldContext = false;
};

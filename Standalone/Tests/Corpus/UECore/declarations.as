enum ECorpusState
{
	Ready,
	Done,
}

struct FCorpusValue
{
	UPROPERTY()
	int Value = 7;
}

int ValidateCorpus()
{
	FCorpusValue Value;
	return Value.Value;
}

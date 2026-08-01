void CompileProjectMapOperations()
{
	TMap<FString, int> Values;
	Values.Add("One", 1);
	bool bContains = Values.Contains("One");
	int FoundValue = 0;
	bool bFound = Values.Find("One", FoundValue);
	int& IndexedValue = Values["One"];
	int& AddedValue = Values.FindOrAdd("Two");
	for (auto Element : Values)
	{
		int Value = Element.GetValue();
		Log("Map contained " + Element.GetKey() + " => " + Element.GetValue());
		Element.SetValue(Value + 1);
	}
	Values.Empty();
}

class AProjectMapOperationsActor : AActor
{
	UPROPERTY()
	TMap<AActor, FString> ActorMap;

	UFUNCTION()
	void CompileActorMapOperations()
	{
		for (auto Element : ActorMap)
		{
			AActor Key = Element.GetKey();
			FString Value = Element.GetValue();
			Log("Actor Map: " + Key);
		}
	}
};

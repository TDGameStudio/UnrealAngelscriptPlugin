delegate void FProjectDelegate(UObject Object, float Value);

void CompileProjectDelegate(FProjectDelegate Value)
{
	bool bBound = Value.IsBound();
	Value.Execute(nullptr, 5.4);
	Value.ExecuteIfBound(nullptr, 1.0);
}

event void FProjectEvent(UObject Object, float Value);

class AProjectDelegateActor : AActor
{
	UPROPERTY()
	FProjectEvent ProjectEvent;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ProjectEvent.Broadcast(nullptr, 100.0);
	}

	UFUNCTION()
	void ProjectFunction(UObject Object, float Value)
	{
		Log("ProjectFunction: " + Value);
	}

	UFUNCTION()
	void CompileBindings()
	{
		ProjectEvent.AddUFunction(this, n"ProjectFunction");
		ProjectEvent.Broadcast(nullptr, 12.5);
		FProjectDelegate LocalDelegate;
		LocalDelegate.BindUFunction(this, n"ProjectFunction");
		CompileProjectDelegate(LocalDelegate);
	}

};

#if TEST
int SelectedValue()
{
	return 41;
}
#else
int SelectedValue()
{
	return missingReleaseSymbol;
}
#endif

int main(const array<string> args)
{
	return SelectedValue();
}

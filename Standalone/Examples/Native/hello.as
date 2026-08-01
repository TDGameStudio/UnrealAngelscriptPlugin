int main(const array<string> args)
{
	const string Name = args.length() > 0 ? args[0] : "world";
	print("hello, " + Name);
	assert(Name.length() > 0, "name must not be empty");
	return 0;
}

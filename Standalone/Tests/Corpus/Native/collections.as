int main(const array<string> args)
{
	assert(args.length() == 0, "unexpected arguments");
	array<int> Values = array<int>();
	Values.insertLast(7);
	Values.insertLast(8);
	Values.insertLast(9);
	dictionary Lookup = dictionary();
	int64 StoredAnswer = 23;
	Lookup.set("answer", StoredAnswer);
	int64 Answer = 0;
	assert(Values.length() == 3, "array length");
	assert(Lookup.get("answer", Answer), "dictionary lookup");
	return int(Answer);
}

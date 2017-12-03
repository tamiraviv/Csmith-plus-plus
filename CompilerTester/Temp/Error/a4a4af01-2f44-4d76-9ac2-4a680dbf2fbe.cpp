
struct S3 {};
struct S4 : S3 {};
struct S5 : virtual S3 {};
struct S7 : S5, S4 {};

int main()
{
	S7 a;

	S7 b;
	b = a;		// works

	S7 c = a;	// doesnt work

	return 0;
}

//Error	C2594	'argument': ambiguous conversions from 'const S7' to 'const S3 &'	

struct S0 
{
	int  a = 1;
};

struct S3 : virtual S0 {};
struct S4 : S3, protected S0 {};
struct S5 : virtual S4, virtual S0 {};

int main()
{
	S5 a;
	S5 b;
	b = a;
}

//Error	C2594	'static_cast': ambiguous conversions from 'const S5' to 'const S0 &'	

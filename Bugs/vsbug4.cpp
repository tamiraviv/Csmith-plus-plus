struct S0 {};
struct S1 : private virtual S0 {};
struct S2 :  S1 {};

int main()
{ 
	S2 a;
}

//Error	C2280	'S2::S2(void)': attempting to reference a deleted function

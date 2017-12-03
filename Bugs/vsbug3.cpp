struct S0 {};
struct S1 : S0 {};
struct S2 : virtual S0, S1 {};  // doesnt work
struct S3 : S1, virtual S0 { }; // works 

int main(int argc, char* argv[]) {}


//Error	C2584	'S2': direct base 'S0' is inaccessible; already a base of 'S1'

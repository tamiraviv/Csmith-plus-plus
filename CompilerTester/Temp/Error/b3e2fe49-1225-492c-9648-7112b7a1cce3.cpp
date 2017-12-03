int f1()
{
	return 1;
}

void f2(unsigned int ui1) {}

int main()
{
	short a = 0;
	f2(a & (f1() | -1));
	return 0;
}

// fatal error C1001: An internal error has occurred in the compiler. 1>  (compiler file 'f:\dd\vctools\compiler\utc\src\p2\main.c', line 255
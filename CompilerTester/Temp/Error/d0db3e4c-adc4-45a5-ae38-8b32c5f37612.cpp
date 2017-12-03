void f(unsigned long b){}

int main()
{
	short av;
	short* a = &av; 
	f(av | ((*a) = 0));
}
 
// fatal error C1001: An internal error has occurred in the compiler. 1>  (compiler file 'f:\dd\vctools\compiler\utc\src\p2\main.c', line 255
//  cl!InvokeCompilerPassW()+0x55a1
//  cl!InvokeCompilerPassW()+0x5392
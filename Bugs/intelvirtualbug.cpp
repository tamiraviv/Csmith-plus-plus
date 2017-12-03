#include <stdio.h>

struct S0 {
	int t = 11;
};

struct S2 : virtual S0 {
};

S2 g_1[1];

int main(int argc, char* argv[])
{
	printf("%d", g_1[0].t);
}

// prints: 0
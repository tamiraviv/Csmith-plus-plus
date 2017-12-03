struct S2
{
   int n = 1;
};

struct S3: virtual S2 {};
struct S4: virtual S3 {};
struct S6: protected S4, virtual S3 {};

int main (int argc, char* argv[])
{
	S6 obj;
	int num = obj.n;

    return 0;
}

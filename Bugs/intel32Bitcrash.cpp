
struct S0 {
};

struct S2 : virtual S0 {
};

S2 g_1[3];

int main(int argc, char* argv[])
{

}

// crashes on 32bit with flag /Od
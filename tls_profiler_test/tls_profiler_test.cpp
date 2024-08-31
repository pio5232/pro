#include <iostream>
#include "TLS_Profiler.h"
#include <thread>

using namespace C_Utility;

void C()
{
	for (int i = 0; i < 4; i++)
	{
		TLS_CHK_START;
		int a = 0;

		int ransd = rand() % 50;

		Sleep(1);
	}
}
void B()
{
	srand(time(nullptr));
	for (int i = 0; i < 3; i++)
	{
		TLS_CHK_START;
		int ransd = rand() % 50;
		Sleep(ransd);
		C();
	}
}
void A(int a)
{
	srand(time(nullptr));

	a *= 10;
	int i = 0;
	while(i++ < a)
	{
		TLS_CHK_START;
		Sleep(10);
		B();
	}
}
int main()
{
	std::thread threads[5];
	{
		TLS_CHK_START;


		for (int i = 0; i < 5; i++)
		{
			threads[i] = std::thread([=]() {A(i+1); });
		}

	}
	for (int i = 0; i < 5; i++)
	{
		if (threads[i].joinable())
			threads[i].join();
	}
	if (TLS_SAVE("saveFile"))
		std::cout << "성공" << std::endl;

	return 0;
}

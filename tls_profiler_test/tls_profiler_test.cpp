#include <iostream>
#include <crtdbg.h>
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

void ADAD()
{
	TLS_CHK_START;

}

void ASD()
{
	TLS_CHK_START;

}
void B(int a)
{
	TLS_CHK_START;


}
void A()
{
	TLS_CHK_START;

}
int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	std::thread threads[5];
	{
		TLS_CHK_START;


		for (int i = 0; i < 5; i++)
		{
			threads[i] = std::thread([=]() {A(i+1); });
		}

	}
	Sleep(300);
	if (TLS_SAVE("SF1"))
		std::cout << "성공" << std::endl;
	
	Sleep(1000);
	printf("-----------------------------------------------------------\n");
	for (int i = 0; i < 5; i++)
	{
		if (threads[i].joinable())
			threads[i].join();
	}
	if (TLS_SAVE("SF2"))
		std::cout << "성공" << std::endl;

	Sleep(1000);
	printf("-----------------------------------------------------------\n");

	for (int i = 0; i < 100; i++)
	{
		A();
		B(4);
		ASD();
		ADAD();
	}
	if (TLS_SAVE("SF3"))
		std::cout << "성공" << std::endl;


	//---------------------------------------------------------
	ProfileBoss::AllResourceRelease();

	return 0;
}

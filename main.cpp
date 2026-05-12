#include <iostream>
using namespace std;

#include "bagel.h"
using namespace bagel;

#include "me_and_dad_model.h"

void run_tests();

int main()
{
	run_tests();

	me_and_dad::createBoard(1);
	me_and_dad::createPlayer({100, 400});
	me_and_dad::createNormalEnemy({300, 400});
	me_and_dad::createSpecialEnemy({500, 400});
	me_and_dad::createThrowableRock({220, 420});

	return 0;
}

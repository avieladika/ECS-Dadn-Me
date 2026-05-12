#include <iostream>
using namespace std;

#include "bagel.h"
using namespace bagel;


bool test_create_entities()
{
	ent_type e1 = World::createEntity();
	if (e1.id != 0)
		return false;
	ent_type e2 = World::createEntity();
	if (e2.id != 1)
		return false;
	ent_type e3 = World::createEntity();
	if (e3.id != 2)
		return false;

	World::deleteEntity(e2);
	World::deleteEntity(e1);
	e2 = World::createEntity();
	if (e2.id != 0)
		return false;

	return true;
}

bool test_world() {
	if (!test_create_entities())
		return false;
	return true;
}

void run_tests()
{
	if (!test_world()) {
		cout << "test_world() failed" << endl;
	}
}

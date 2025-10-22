#include "TestObject.h"


TestObject::TestObject()
	: m_data(0)
{
	std::cout << "TestObject Constructor Called\n";
}

TestObject::~TestObject()
{
	std::cout << "TestObject Destructor Called\n";
}

#include <iostream>
#include <raul/Thread.h>

using namespace std;

int
main()
{
	Thread& this_thread = Thread::get();
	this_thread.set_name("Main");

	cerr << "Thread name: " << Thread::get().name() << endl;

	return 0;
}


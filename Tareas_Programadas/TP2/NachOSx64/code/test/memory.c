#include "syscall.h"

int array[16];

int main() {
	int i;
	for (i = 0; i < 16; i++) 
		array[i] = 2025;

	Exit(0);
}


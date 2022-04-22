#include <time.h>

void Interval(double i) {
	clock_t g;
	g = i * CLOCKS_PER_SEC + clock();
	while (g > clock());
}
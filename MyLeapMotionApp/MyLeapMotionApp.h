#pragma once

#include "resource.h"

void Interval(double);
void Startleap();
void Closeleap();
struct Handpositions getposition(int);

struct Handpositions {
	char left;
	float x = 0;
	float y = 0;
	float z = 0;
};

static struct Handpositions hands[2];

#pragma once
#include <random>

using Address = int;

Address CreateRandomAddress()
{
	return rand();
}

static std::random_device rd;
static std::mt19937 gen(rd());
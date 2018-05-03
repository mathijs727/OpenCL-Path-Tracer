#pragma once
#include "types.h"

union PixelRGBA
{
	u32 value;
	struct
	{
		u8 b;
		u8 g;
		u8 r;
		u8 a;
	};
};


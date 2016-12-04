#ifndef __GAMMA_CL
#define __GAMMA_CL

float3 accurateLinearToSRGB(float3 linearCol)
{
	float3 sRGBLo = linearCol * 12.92f;
	float3 sRGBHi = (pow(fabs(linearCol), 1.0f / 2.4f) * 1.055f) - 0.055f;
	float3 sRGB;
	sRGB.x = (linearCol.x <= 0.0031308f) ? sRGBLo.x : sRGBHi.x;
	sRGB.y = (linearCol.x <= 0.0031308f) ? sRGBLo.y : sRGBHi.y;
	sRGB.z = (linearCol.x <= 0.0031308f) ? sRGBLo.z : sRGBHi.z;
	return sRGB;
}

#endif
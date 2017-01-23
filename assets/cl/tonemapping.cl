#ifndef __TONEMAPPING_CL
#define __TONEMAPPING_CL

// Reinhard tonemapping as described here:
// http://filmicgames.com/archives/75
// http://www.gdcvault.com/play/1012351/Uncharted-2-HDR
float3 tonemapReinhard(float3 luminance)
{
	return luminance / (1.0f + luminance);
}

float3 tonemapUncharted2(float3 luminance)
{
	// Supposed to be the filmic tone mapping that was used in Uncharted 2.
	// Naughty dog did a presentation on this, but did not describe the actual algorithm:
	// http://www.gdcvault.com/play/1012351/Uncharted-2-HDR

	// So this monster formula is copied from:
	// http://filmicgames.com/archives/75
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	float3 x = luminance;

	return luminance / ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

#endif // __TONEMAPPING_CL
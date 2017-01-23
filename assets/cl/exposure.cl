#ifndef EXPOSURE_CL
#define EXPOSURE_CL
#include "camera.cl"

// https://placeholderart.wordpress.com/2014/11/16/implementing-a-physically-based-camera-understanding-exposure
/// Exposure (not the same as tone mapping!)
float computeEV100(float aperture, float shutterTime, float ISO)
{
	// EV number is defined as:
	// 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
	// This gives
	// EV_s = log2 (N^2 / t)
	// EV_100 + log2 (S /100) = log2 (N^2 / t)
	// EV_100 = log2 (N^2 / t) - log2 (S /100)
	// EV_100 = log2 (N^2 / t . 100 / S)
	return log2(sqrt(aperture) / shutterTime * 100 / ISO);
}

float computeEV100FromAvgLuminance(float avgLuminance )
{
	// We later use the middle gray at 12.7% in order to have
	// a middle gray at 18% with a sqrt (2) room for specular highlights
	// But here we deal with the spot meter measuring the middle gray
	// which is fixed at 12.5 for matching standard camera
	// constructor settings (i.e. calibration constant K = 12.5)
	// Reference : http :// en. wikipedia . org / wiki / Film_speed
	return log2(avgLuminance * 100.0f / 12.5f);
}

float convertEV100toExposure(float EV100)
{
	// Compute the maximum luminance possible with H_sbs sensitivity
	// maxLum = 78 / ( S * q ) * N^2 / t
	// = 78 / ( S * q ) * 2^ EV_100
	// = 78 / (100 * 0.65) * 2^ EV_100
	// = 1.2 * 2^ EV
	// Reference : http :// en. wikipedia . org / wiki / Film_speed
	float maxLuminance = 1.2f * pow (2.0f, EV100);
	return 1.0f / maxLuminance;
}

float3 calcExposedLuminance(__global const Camera* camera, float3 luminance)
{
	// usage with manual settings
	float EV100 = computeEV100(camera->relativeAperture, camera->shutterTime, camera->ISO);
	
	// Calculating Lavg is expensive, could probably not change it after x number of iterations (because image converges)
	// But not worth teh time right now.

	// usage with auto settings
	//float AutoEV100 = computeEV100FromAvgLuminance(Lavg);
	//float currentEV = useAutoExposure ? AutoEV100 : EV100;

	float currentEV = EV100;
	float exposure = convertEV100toExposure(currentEV);

	// exposure can then be used later in the shader to scale luminance
	// if color is decomposed into XYZ
	return luminance * exposure;
}

#endif // EXPOSURE_CL
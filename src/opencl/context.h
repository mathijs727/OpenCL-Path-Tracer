#pragma once
#include <opencl/cl12.hpp>

class CLContext {
public:
    CLContext(cl::Context context, cl::Device device, cl::CommandQueue graphicsQueue, cl::CommandQueue copyQueue);
    ~CLContext() = default;

    cl::Context getContext() const;
    operator cl::Context() const;
    cl::Device getDevice() const;
    cl::CommandQueue getGraphicsQueue() const;
    cl::CommandQueue getCopyQueue() const;

private: // TMP
    cl::Context m_context;
    cl::Device m_device;
    cl::CommandQueue m_graphicsQueue;
    cl::CommandQueue m_copyQueue;
};

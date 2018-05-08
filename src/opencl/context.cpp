#include "context.h"

CLContext::CLContext(cl::Context context, cl::Device device, cl::CommandQueue graphicsQueue, cl::CommandQueue copyQueue) :
    m_context(context),
    m_device(device),
    m_graphicsQueue(graphicsQueue),
    m_copyQueue(copyQueue)
{
}

cl::Context CLContext::getContext() const
{
    return m_context;
}

CLContext::operator cl::Context() const
{
    return m_context;
}

cl::Device CLContext::getDevice() const
{
    return m_device;
}

cl::CommandQueue CLContext::getGraphicsQueue() const
{
    return m_graphicsQueue;
}

cl::CommandQueue CLContext::getCopyQueue() const
{
    return m_copyQueue;
}


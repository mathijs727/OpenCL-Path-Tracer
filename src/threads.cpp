// Template, major revision 6, update for INFOMOV
// IGAD/NHTV/UU - Jacco Bikker - 2006-2016

#include "template.h"
#include <thread>


using namespace Tmpl8;


void JobThread::CreateAndStartThread(unsigned int threadId)
{
	m_Go = false;
	m_ThreadID = threadId;
	m_Thread = std::thread(&JobThread::BackgroundTask, this);
}

void JobThread::BackgroundTask()
{
	while (1)
	{
		while (!m_Go) {
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
		}
		m_Go = false;

		while (1)
		{
			Job* job = JobManager::GetJobManager()->GetNextJob();
			if (!job)
			{
				JobManager::GetJobManager()->ThreadDone(m_ThreadID);
				break;
			}
			std::cout << "Running job" << std::endl;

			job->RunCodeWrapper();
		}
	}
}

void JobThread::Go()
{
	m_Go = true;
}

void Job::RunCodeWrapper()
{
	Main();
}
JobManager* JobManager::m_JobManager = 0;

JobManager::JobManager(unsigned int threads) : m_NumThreads(threads)
{
}

JobManager::~JobManager()
{
}

void JobManager::CreateJobManager(unsigned int numThreads)
{
	m_JobManager = new JobManager(numThreads);
	m_JobManager->m_JobThreadList = new JobThread[numThreads];
	for (unsigned int i = 0; i < numThreads; i++)
	{
		m_JobManager->m_JobThreadList[i].CreateAndStartThread(i);
		m_JobManager->m_ThreadDone[i] = false;
	}
	m_JobManager->m_JobCount = 0;
}

void JobManager::AddJob2(Job* a_Job)
{
	m_JobList[m_JobCount++] = a_Job;
}

Job* JobManager::GetNextJob()
{
	Job* job = 0;
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_JobCount > 0) job = m_JobList[--m_JobCount];
	return job;
}

void JobManager::RunJobs()
{
	for (unsigned int i = 0; i < m_NumThreads; i++)
	{
		m_ThreadDone[i] = false;
		m_JobThreadList[i].Go();
	}

	for (unsigned int i = 0; i < m_NumThreads; i++)
	{
		while (!m_ThreadDone[i]) {
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
		}
	}
}

void JobManager::ThreadDone(unsigned int n)
{
	m_ThreadDone[n] = true;
}

// EOF
#include "Logging/MemoryLogSink.h"

#include "Core/Assert.h"

#include <cstddef>

FMemoryLogSink::FMemoryLogSink(int32 InCapacity)
	: Capacity(InCapacity)
{
	WW_CHECKF(InCapacity >= 1, "A memory log sink needs a capacity of at least 1, not {}", InCapacity);
}

void FMemoryLogSink::Write(const FLogMessage& Message)
{
	const std::scoped_lock Lock(Mutex);

	if (Messages.size() >= static_cast<std::size_t>(Capacity))
	{
		Messages.pop_front();
	}
	Messages.push_back(Message);
}

std::vector<FLogMessage> FMemoryLogSink::GetMessages() const
{
	const std::scoped_lock Lock(Mutex);

	return std::vector<FLogMessage>(Messages.begin(), Messages.end());
}

void FMemoryLogSink::Clear()
{
	const std::scoped_lock Lock(Mutex);

	Messages.clear();
}

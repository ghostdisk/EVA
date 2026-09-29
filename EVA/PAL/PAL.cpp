#include <EVA/PAL/PAL.hpp>
#include <vector>

namespace EVA::PAL
{

std::vector<Event> pending_events;

void InitBackend();
void PollBackend();

void Init()
{
	InitBackend();
}

bool Poll(Event* out_event)
{
	if (pending_events.empty())
		PollBackend();

	if (pending_events.size() > 0)
	{
		*out_event = pending_events[0];
		pending_events.erase(pending_events.begin());
		return true;
	}
	else
	{
		return false;
	}
}

bool HasPendingEvents()
{
	return !pending_events.empty();
}

void EmitEvent(Event event)
{
	pending_events.push_back(event);
}

}

#ifndef PY_NETWORK_HPP
#define PY_NETWORK_HPP

#include "network/event_dispatcher.hpp"
#include "network/network_interface.hpp"

namespace PyNetwork
{
	bool init( Mercury::Nub & dispatcher,
			Mercury::Nub & interface );
}

#endif // PY_NETWORK_HPP

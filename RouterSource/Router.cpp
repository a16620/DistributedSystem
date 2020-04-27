#include "Router.h"
#include "Packet.h"
#include <random>

void Router::Update(const Address& destination, Serial* port, size_t distance)
{
	auto table = routingTable.find(destination);
	if (table == routingTable.end()) {
		auto tmp = routingTable.insert(std::make_pair(destination, Table(4)));
		tmp.first->second.Insert(port, distance);
		it = tmp.first;
	}
	else
		table->second.Insert(port, distance);
}

void Router::Detach(Serial* port)
{
	for (auto table = routingTable.begin(); table != routingTable.end();) {
		table->second.Remove(port);
		if (table->second.empty()) {
			table = routingTable.erase(table);
		}
		else
			++table;
	}
}

void Router::RemoveAddress(const Address& address)
{
	routingTable.erase(address);
}

bool Router::Query(const Address& destination, Serial** out)
{
	auto it = routingTable.find(destination);
	if (it != routingTable.end()) {
		const auto& table = it->second;
		if (!table.empty()) {
			*out = table.get();
			return true;
		}
	}
	return false;
}

bool Router::IsSuper(const Address& destination, Serial* port)
{
	//TODO
	return false;
}

Router::Router()
{
	it = routingTable.end();
}

bool Router::GetRandomRoute(Router::dvec& vec)
{
	if (routingTable.size() == 0)
		return false;
	
	if (it == routingTable.end())
		it = routingTable.begin();

	vec = dvec{ it->first, it->second.dist() };
	++it;
	return true;
}

void Table::Insert(Serial* port, size_t distance)
{
	auto it = rtable.find(port);
	if (it == rtable.end()) {
		table.insert(std::make_pair(distance, std::unordered_set<Serial*>({port})));
		rtable.insert(std::make_pair(port, distance));
	}
	else {
		if (it->second == distance)
			return;
		Remove(port);

		rtable.insert(std::make_pair(port, distance));

		auto n = table.find(distance);
		if (n == table.end()) {
			table.insert(std::make_pair(distance, std::unordered_set<Serial*>({ port })));
		}
		else {
			n->second.insert(port);
		}
	}

	if (rtable.size() > maxLength) {
		Remove(*((--table.end())->second.begin()));
	}
}

void Table::Remove(Serial* port)
{
	auto it = rtable.find(port);
	if (it != rtable.end()) {
		auto at = table.find(it->second);
		at->second.erase(port);
		rtable.erase(it);
		if (at->second.empty()) {
			table.erase(at);
		}
	}
}

bool Table::empty() const
{
	return rtable.empty();
}

size_t Table::dist() const
{
	return table.begin()->first;
}

Serial* Table::get() const
{
	return *(table.begin()->second.begin());
}

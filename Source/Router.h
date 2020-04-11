#pragma once
#include <unordered_map>
#include <map>
#include <memory>
#include <unordered_set>
#include "def.h"
#include "Serial.h"
#include "SocketSerial.h"

class Table;

class Router
{
	std::unordered_map<Address, Table> routingTable;
	int counter;
public:
	void Update(Address destination, Serial* port, size_t distance);
	void Detach(Serial* port);
	void RemoveAddress(Address address);
	bool Query(Address destination, Serial** out);

public:
	struct dvec {
		Address address;
		size_t distance;
	};
	dvec GetRandomRoute();
};

class Table {
	std::multimap<size_t, std::unordered_set<Serial*>> table;
	std::unordered_map<Serial*, size_t> rtable;
	const size_t maxLength;
public:
	Table(size_t maxLength) : maxLength(maxLength) {}
	void Insert(Serial* port, size_t distance);
	void Remove(Serial* port);
	bool empty() const;
	size_t dist() const;
	Serial* get() const;
};
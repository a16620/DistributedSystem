#pragma once
class Serial
{
public:
	virtual int Write(const char* buffer, int lenth)=0;
	virtual int Read(char* buffer, int length) = 0;

	virtual ~Serial() {}
};


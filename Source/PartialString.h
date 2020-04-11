#pragma once
#include <iostream>
#include <atomic>
#include <vector>
using namespace std;

struct _PartialStringDataBlock {
	char* origin;
	atomic_size_t referenceCount;

	void Detach() {
		referenceCount--;
		if (referenceCount == 0) {
			delete this;
		}
	}

	~_PartialStringDataBlock() {
		delete[] origin;
	}
};

class PartialString {
private:
	_PartialStringDataBlock* datablock;
	char* begin;
	size_t length;

	PartialString(_PartialStringDataBlock* datablock, char* begin, size_t length) : datablock(datablock), begin(begin), length(length) {
		datablock->referenceCount++;
	}

public:

	const char* getString() const {
		return begin;
	}

	template <template<typename, typename> typename iList>
	static auto Split(const string& origin, char d) {
		using Container = iList<PartialString, allocator<PartialString>>;
		if (origin.empty()) {
			throw "empty string";
		}

		char* str = nullptr;
		_PartialStringDataBlock* datablock = nullptr;

		try {
			const size_t szStr = origin.length();
			str = new char[szStr];
			datablock = new _PartialStringDataBlock;
			datablock->origin = str;
			datablock->referenceCount = 1;

			memcpy(str, origin.c_str(), szStr);

			Container list;
			int offset = 0;
			for (int i = 1; i < szStr; ++i) {
				if (origin[i] == d) {
					size_t pLen = i - offset;
					PartialString tStr(datablock, str + offset, pLen);
					list.push_back(tStr); //It isn't using emplace_back as the constructor is private.
					offset = i+1;
					str[i] = '\0';
				}
			}

			datablock->Detach();

			return list;
		}
		catch (exception e) {
			if (datablock != nullptr) {
				delete str;
				delete datablock;
			}
			else if (str != nullptr) {
				delete str;
			}

			throw e;
		}
	}

	PartialString(PartialString&& o) noexcept {
		datablock = o.datablock;
		begin = o.begin;
		length = o.length;

		o.datablock = nullptr;
		o.begin = nullptr;
		o.length = 0;
	}

	PartialString(const PartialString& o) {
		datablock = o.datablock;
		begin = o.begin;
		length = o.length;

		datablock->referenceCount++;
	}

	~PartialString() {
		if (datablock != nullptr)
			datablock->Detach();
	}
};
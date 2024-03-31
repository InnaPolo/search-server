#pragma once
#include <iostream>
#include <vector>
#include <stdexcept>

#include "document.h"

template<typename Iterator>
class IteratorRange {
public:
	explicit IteratorRange(Iterator begin, Iterator end)
		:begin_(begin), end_(end) {}

	Iterator begin() const {
		return begin_;
	}
	Iterator end() const {
		return end_;
	}
	size_t size() const {
		return  distance(begin_, end_);
	}
private:
	Iterator begin_;
	Iterator end_;
};

template <typename Iterator>
class Paginator {
public:
	explicit Paginator(Iterator range_begin, Iterator range_end, size_t page_size);

	~Paginator() {}

	auto begin() const {
		return pages.begin();
	}
	auto end() const {
		return pages.end();
	}
	size_t size() const {
		return pages.size();
	}
private:
	std::vector<IteratorRange<Iterator>> pages;
};

template<typename Iterator>
std::ostream& operator<< (std::ostream& os, const IteratorRange<Iterator>& page) 
{
	for (auto i = page.begin(); i != page.end(); advance(i, 1)) {
		os << *i;
	}
	return os;
}

template <typename Iterator>
Paginator<Iterator>::Paginator(Iterator range_begin, Iterator range_end, size_t page_size)
{
	if (page_size == 0) {
		throw std::out_of_range("Size is zero page");
	}
	if (range_begin == range_end) {
		return;
	}
	while (distance(range_begin, range_end) > static_cast<int>(page_size)) {
		auto it = range_begin;
		advance(it, page_size);
		pages.push_back(IteratorRange(range_begin, it));
		range_begin = it;
	}
	pages.push_back(IteratorRange(range_begin, range_end));
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size)
{
	return Paginator(begin(c), end(c), page_size);
}
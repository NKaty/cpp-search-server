#pragma once

#include <vector>
#include <algorithm>
#include <iostream>

template<typename Iterator>
class IteratorRange {
 public:
  explicit IteratorRange(Iterator range_begin, Iterator range_end);

  Iterator begin() const;

  Iterator end() const;

  size_t size() const;

 private:
  Iterator range_begin_;
  Iterator range_end_;
  size_t size_;
};

template<typename Iterator>
class Paginator {
 public:
  explicit Paginator(Iterator range_begin, Iterator range_end, size_t page_size);

  auto begin() const;

  auto end() const;

  size_t size() const;

 private:
  std::vector<IteratorRange<Iterator>> pages_;
};

template<typename Iterator>
std::ostream &operator<<(std::ostream &os, const IteratorRange<Iterator> &iteratorRange);

template<typename Container>
auto Paginate(const Container &c, size_t page_size);

template<typename Iterator>
IteratorRange<Iterator>::IteratorRange(Iterator range_begin, Iterator range_end)
    : range_begin_(range_begin), range_end_(range_end),
      size_(distance(range_begin, range_end)) {
}

template<typename Iterator>
Iterator IteratorRange<Iterator>::begin() const {
  return range_begin_;
}

template<typename Iterator>
Iterator IteratorRange<Iterator>::end() const {
  return range_end_;
}

template<typename Iterator>
size_t IteratorRange<Iterator>::size() const {
  return size_;
}

template<typename Iterator>
Paginator<Iterator>::Paginator(Iterator range_begin, Iterator range_end, size_t page_size) {
  size_t items_left = distance(range_begin, range_end);
  while (items_left) {
    const auto item_size = std::min(page_size, items_left);
    const auto range_begin_copy = range_begin;
    advance(range_begin, item_size);
    pages_.push_back(IteratorRange<Iterator>(range_begin_copy, range_begin));
    items_left -= item_size;
  }
}

template<typename Iterator>
auto Paginator<Iterator>::begin() const {
  return pages_.begin();
}

template<typename Iterator>
auto Paginator<Iterator>::end() const {
  return pages_.end();
}

template<typename Iterator>
size_t Paginator<Iterator>::size() const {
  return pages_.size();
}

template<typename Iterator>
std::ostream &operator<<(std::ostream &os, const IteratorRange<Iterator> &iteratorRange) {
  for (auto it = iteratorRange.begin(); it != iteratorRange.end(); it = next(it)) {
    os << *it;
  }
  return os;
}

template<typename Container>
auto Paginate(const Container &c, size_t page_size) {
  return Paginator(begin(c), end(c), page_size);
}

#pragma once

#include "document.h"
#include "search_server.h"

#include <string>
#include <vector>
#include <deque>

class RequestQueue {
 public:
  explicit RequestQueue(const SearchServer &search_server);

  template<typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string &raw_query,
                                       DocumentPredicate document_predicate);

  std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);

  std::vector<Document> AddFindRequest(const std::string &raw_query);

  int GetNoResultRequests() const;

 private:
  struct QueryResult {
    uint64_t timestamp;
    bool isEmpty;
  };

  std::deque<QueryResult> requests_;
  const static int min_in_day_ = 1440;
  const SearchServer &search_server_;
  uint64_t time_count;
  int noResultsRequestsCount = 0;

  void OnNewRequest(bool isResultEmpty);
};

template<typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query,
                                                   DocumentPredicate document_predicate) {
  auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
  OnNewRequest(result.empty());
  return result;
}

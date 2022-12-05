#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer &search_server) : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status) {
  auto result = search_server_.FindTopDocuments(raw_query, status);
  OnNewRequest(result.empty());
  return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
  auto result = search_server_.FindTopDocuments(raw_query);
  OnNewRequest(result.empty());
  return result;
}

int RequestQueue::GetNoResultRequests() const {
  return noResultsRequestsCount;
}

void RequestQueue::OnNewRequest(bool isResultEmpty) {
  ++time_count;
  if (!requests_.empty() && time_count - requests_.front().timestamp >= min_in_day_) {
    if (requests_.front().isEmpty) {
      --noResultsRequestsCount;
    }
    requests_.pop_front();
  }
  requests_.push_back({time_count, isResultEmpty});
  if (isResultEmpty) {
    ++noResultsRequestsCount;
  }
}

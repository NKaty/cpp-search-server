#pragma once

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <execution>
#include <numeric>

using namespace std::string_literals;

#define ERROR_MARGIN 1e-6

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
 public:
  template<typename StringContainer>
  explicit SearchServer(const StringContainer &stop_words);

  explicit SearchServer(const std::string &stop_words_text);

  explicit SearchServer(std::string_view stop_words_text);

  void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                   const std::vector<int> &ratings);

  template<typename DocumentPredicate>
  std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                         DocumentPredicate document_predicate) const;

  template<typename DocumentPredicate, typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                         std::string_view raw_query,
                                         DocumentPredicate document_predicate) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

  template<typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                         std::string_view raw_query,
                                         DocumentStatus status) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

  template<typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
                                         std::string_view raw_query) const;

  int GetDocumentCount() const;

  std::tuple<std::vector<std::string_view>,
             DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
      const std::execution::sequenced_policy &policy,
      std::string_view raw_query,
      int document_id) const;

  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
      const std::execution::parallel_policy &policy,
      std::string_view raw_query,
      int document_id) const;

  const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

  void RemoveDocument(int document_id);

  template<typename ExecutionPolicy>
  void RemoveDocument(ExecutionPolicy &&policy, int document_id);

  std::set<int>::const_iterator begin() const;

  std::set<int>::const_iterator end() const;

 private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  struct QueryWord {
    std::string_view data;
    bool is_minus;
    bool is_stop;
  };

  struct Query {
    std::vector<std::string_view> plus_words;
    std::vector<std::string_view> minus_words;
  };

  const std::set<std::string, std::less<>> stop_words_;
  std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
  std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
  std::map<int, DocumentData> documents_;
  std::set<int> document_ids_;

  bool IsStopWord(std::string_view word) const;

  std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

  QueryWord ParseQueryWord(std::string_view text) const;

  Query ParseQueryUnique(std::string_view raw_query) const;

  Query ParseQuery(std::string_view raw_query) const;

  Query GetValidParsedQuery(std::string_view raw_query, bool uniqueWords = true) const;

  // Existence required
  double ComputeWordInverseDocumentFreq(std::string_view word) const;

  template<typename DocumentPredicate>
  std::vector<Document> FindAllDocuments(const Query &query,
                                         DocumentPredicate document_predicate) const;

  template<typename DocumentPredicate, typename ExecutionPolicy>
  std::vector<Document> FindAllDocuments(ExecutionPolicy &&policy,
                                         const Query &query,
                                         DocumentPredicate document_predicate) const;

  static int ComputeAverageRating(const std::vector<int> &ratings);

  static bool IsValidWord(std::string_view word);
};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
  if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
    throw std::invalid_argument("Stop words contain forbidden symbols"s);
  }
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
  return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                                                     std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
  const Query query = GetValidParsedQuery(raw_query);

  auto matched_documents = FindAllDocuments(policy, query, document_predicate);

  sort(
      policy,
      matched_documents.begin(),
      matched_documents.end(),
      [](const Document &lhs, const Document &rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < ERROR_MARGIN) {
          return lhs.rating > rhs.rating;
        } else {
          return lhs.relevance > rhs.relevance;
        }
      });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }
  return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query,
                                                     DocumentPredicate document_predicate) const {
  return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy &&policy,
                                                     const Query &query,
                                                     DocumentPredicate document_predicate) const {
  ConcurrentMap<int, double>
      concurrent_map_document_to_relevance(std::thread::hardware_concurrency());
  std::for_each(
      policy,
      query.plus_words.begin(),
      query.plus_words.end(),
      [this, document_predicate, &concurrent_map_document_to_relevance](std::string_view word) {
        const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
        if (word_to_document_freqs_it != word_to_document_freqs_.end()) {
          const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
          for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
            const auto &document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
              concurrent_map_document_to_relevance[document_id].ref_to_value +=
                  term_freq * inverse_document_freq;
            }
          }
        }
      }
  );

  std::for_each(
      policy,
      query.minus_words.begin(),
      query.minus_words.end(),
      [this, &concurrent_map_document_to_relevance](std::string_view word) {
        const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
        if (word_to_document_freqs_it != word_to_document_freqs_.end()) {
          for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
            concurrent_map_document_to_relevance.erase(document_id);
          }
        }
      }
  );

  auto document_to_relevance = concurrent_map_document_to_relevance.BuildOrdinaryMap();

  std::vector<Document> matched_documents;
  matched_documents.reserve(document_to_relevance.size());
  for (const auto [document_id, relevance] : document_to_relevance) {
    matched_documents.push_back(
        {document_id, relevance, documents_.at(document_id).rating});
  }
  return matched_documents;
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                                                     std::string_view raw_query,
                                                     DocumentStatus status) const {
  return FindTopDocuments(policy,
                          raw_query,
                          [status](int document_id, DocumentStatus document_status, int rating) {
                            return document_status == status;
                          });
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
                                                     std::string_view raw_query) const {
  return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy &&policy, int document_id) {
  const auto document_to_word_freqs_it = document_to_word_freqs_.find(document_id);
  if (document_to_word_freqs_it == document_to_word_freqs_.end()) {
    return;
  }
  std::vector<std::string_view> words;
  std::transform(
      document_to_word_freqs_it->second.begin(),
      document_to_word_freqs_it->second.end(),
      std::back_inserter(words),
      [](const auto &word_freqs) { return word_freqs.first; });
  document_ids_.erase(document_id);
  documents_.erase(document_id);
  document_to_word_freqs_.erase(document_id);
  std::for_each(
      policy,
      words.begin(),
      words.end(),
      [this, document_id](const auto &word) {
        const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
        if (word_to_document_freqs_it != word_to_document_freqs_.end()) {
          word_to_document_freqs_it->second.erase(document_id);
        }
      }
  );
}

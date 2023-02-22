#include "search_server.h"

#include <numeric>
#include <cmath>

using namespace std;

SearchServer::SearchServer(string_view stop_words_text) : SearchServer(
    SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const string &stop_words_text) : SearchServer(
    string_view(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const vector<int> &ratings) {
  if (document_id < 0) {
    throw invalid_argument("Document id must not be negative"s);
  }
  if (documents_.count(document_id)) {
    throw invalid_argument("Document with id "s + to_string(document_id) + " already exists"s);
  }
  if (!IsValidWord(document)) {
    throw invalid_argument("Document contains forbidden symbols"s);
  }
  const vector<string_view> words = SplitIntoWordsNoStop(document);
  const double inv_word_count = 1.0 / words.size();
  for (const string_view word : words) {
    word_to_document_freqs_[string(word)][document_id] += inv_word_count;
    document_to_word_freqs_[document_id][string(word)] += inv_word_count;
  }
  documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
  document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query,
                                                DocumentStatus status) const {
  return FindTopDocuments(
      raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
      });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
  return document_ids_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query,
                                                                       int document_id) const {
//  LOG_DURATION_STREAM("Operation time", cout);
  if (document_id < 0 || !document_ids_.count(document_id)) {
    throw out_of_range("Document is invalid"s);
  }
  const Query query = GetValidParsedQuery(raw_query);
  for (string_view word : query.minus_words) {
    const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
    if (word_to_document_freqs_it == word_to_document_freqs_.end()) {
      continue;
    }
    if (word_to_document_freqs_it->second.count(document_id)) {
      return {vector<string_view>{}, documents_.at(document_id).status};
    }
  }
  vector<string_view> matched_words;
  for (string_view word : query.plus_words) {
    const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
    if (word_to_document_freqs_it == word_to_document_freqs_.end()) {
      continue;
    }
    if (word_to_document_freqs_it->second.count(document_id)) {
      matched_words.push_back(word);
    }
  }
  return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const execution::sequenced_policy &policy,
    string_view raw_query,
    int document_id) const {
  return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const execution::parallel_policy &policy,
    string_view raw_query,
    int document_id) const {
  if (document_id < 0 || !document_ids_.count(document_id)) {
    throw out_of_range("Document is invalid"s);
  }
  const Query query = GetValidParsedQuery(raw_query, false);

  if (any_of(
      policy,
      query.minus_words.begin(),
      query.minus_words.end(),
      [this, document_id](string_view word) {
        const auto it = word_to_document_freqs_.find(word);
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
      }
  )) {
    return {vector<string_view>{}, documents_.at(document_id).status};
  }
  vector<string_view> matched_words(query.plus_words.size());
  transform(
      policy,
      query.plus_words.begin(),
      query.plus_words.end(),
      matched_words.begin(),
      [this, document_id](string_view word) {
        const auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id)) {
          return word;
        }
        return string_view{};
      }
  );
  sort(
      policy,
      matched_words.begin(),
      matched_words.end(),
      greater<>());

  matched_words.erase(
      unique(
          policy,
          matched_words.begin(),
          matched_words.end()),
      matched_words.end());
  if (!matched_words.empty() && matched_words[matched_words.size() - 1].empty()) {
    matched_words.pop_back();
  }
  return {matched_words, documents_.at(document_id).status};
}

const map<string_view, double> &SearchServer::GetWordFrequencies(int document_id) const {
  const auto it = document_to_word_freqs_.find(document_id);
  if (it == document_to_word_freqs_.end()) {
    static const map<string_view, double> empty_map;
    return empty_map;
  }
  return it->second;
}

void SearchServer::RemoveDocument(int document_id) {
  return RemoveDocument(execution::seq, document_id);
}

set<int>::const_iterator SearchServer::begin() const {
  return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
  return document_ids_.end();
}

bool SearchServer::IsStopWord(string_view word) const {
  return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
  vector<string_view> words;
  for (const string_view word : SplitIntoWords(text)) {
    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
  if (ratings.empty()) {
    return 0;
  }
  int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
  return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
  bool is_minus = false;
  // Word shouldn't be empty
  if (text[0] == '-') {
    is_minus = true;
    text = text.substr(1);
  }
  return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQueryUnique(string_view text) const {
  Query query;
  set<string_view> plus_words;
  set<string_view> minus_words;
  for (string_view word : SplitIntoWords(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        minus_words.insert(query_word.data);
      } else {
        plus_words.insert(query_word.data);
      }
    }
  }
  return {{plus_words.begin(), plus_words.end()}, {minus_words.begin(), minus_words.end()}};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
  Query query;
  for (string_view word : SplitIntoWords(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.push_back(query_word.data);
      } else {
        query.plus_words.push_back(query_word.data);
      }
    }
  }
  return query;
}

SearchServer::Query SearchServer::GetValidParsedQuery(string_view raw_query,
                                                      bool uniqueWords) const {
  if (!IsValidWord(raw_query)) {
    throw invalid_argument("Query contains forbidden symbols"s);
  }
  Query query = uniqueWords ? ParseQueryUnique(raw_query) : ParseQuery(raw_query);
  for (auto &word : query.minus_words) {
    if (word.empty() || word[0] == '-') {
      throw invalid_argument("Invalid query"s);
    }
  }
  return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
  return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(string(word)).size());
}

bool SearchServer::IsValidWord(string_view word) {
  return none_of(word.begin(), word.end(), [](char c) {
    return c >= '\0' && c < ' ';
  });
}

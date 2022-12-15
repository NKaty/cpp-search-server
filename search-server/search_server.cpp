#include "search_server.h"

#include <numeric>
#include <cmath>

using namespace std;

SearchServer::SearchServer(const string &stop_words_text) : SearchServer(
    SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const string &document, DocumentStatus status,
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
  const vector<string> words = SplitIntoWordsNoStop(document);
  const double inv_word_count = 1.0 / words.size();
  for (const string &word : words) {
    word_to_document_freqs_[word][document_id] += inv_word_count;
    document_to_word_freqs_[document_id][word] += inv_word_count;
  }
  documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
  document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query,
                                                DocumentStatus status) const {
  return FindTopDocuments(
      raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
      });
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
  return document_ids_.size();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string &raw_query,
                                                                  int document_id) const {
//  LOG_DURATION_STREAM("Operation time", cout);
  const Query query = GetValidParsedQuery(raw_query);
  vector<string> matched_words;
  for (const string &word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id)) {
      matched_words.push_back(word);
    }
  }
  for (const string &word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id)) {
      matched_words.clear();
      break;
    }
  }
  return {matched_words, documents_.at(document_id).status};
}

const map<string, double> &SearchServer::GetWordFrequencies(int document_id) const {
  const auto it = document_to_word_freqs_.find(document_id);
  if (it == document_to_word_freqs_.end()) {
    static const map<string, double> empty_map;
    return empty_map;
  }
  return it->second;
}

void SearchServer::RemoveDocument(int document_id) {
  const auto document_to_word_freqs_it = document_to_word_freqs_.find(document_id);
  if (document_to_word_freqs_it == document_to_word_freqs_.end()) {
    return;
  }
  const auto word_freqs = document_to_word_freqs_it->second;
  document_ids_.erase(document_id);
  documents_.erase(document_id);
  document_to_word_freqs_.erase(document_id);
  for (const auto &[word, freq] : word_freqs) {
    const auto word_to_document_freqs_it = word_to_document_freqs_.find(word);
    if (word_to_document_freqs_it != word_to_document_freqs_.end()) {
      word_to_document_freqs_it->second.erase(document_id);
    }
  }
}

set<int>::const_iterator SearchServer::begin() const {
  return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
  return document_ids_.end();
}

bool SearchServer::IsStopWord(const string &word) const {
  return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string &text) const {
  vector<string> words;
  for (const string &word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
  bool is_minus = false;
  // Word shouldn't be empty
  if (text[0] == '-') {
    is_minus = true;
    text = text.substr(1);
  }
  return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const string &text) const {
  Query query;
  for (const string &word : SplitIntoWords(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.insert(query_word.data);
      } else {
        query.plus_words.insert(query_word.data);
      }
    }
  }
  return query;
}

SearchServer::Query SearchServer::GetValidParsedQuery(const string &raw_query) const {
  if (!IsValidWord(raw_query)) {
    throw invalid_argument("Query contains forbidden symbols"s);
  }
  Query query = ParseQuery(raw_query);
  for (auto &word : query.minus_words) {
    if (word.empty() || word[0] == '-') {
      throw invalid_argument("Invalid query"s);
    }
  }
  return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string &word) const {
  return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string &word) {
  return none_of(word.begin(), word.end(), [](char c) {
    return c >= '\0' && c < ' ';
  });
}

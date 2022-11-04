#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

struct QueryWord {
    string data;
    bool minus_word;
    bool stop_word;
};

struct Query {
    set<string> plus_words;
    set<string> minus_words;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        double freq = 1.0 / words.size();
        for (const auto& word: words) {
            word_to_document_freqs_[word][document_id] += freq;
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    map<string, map<int, double>> word_to_document_freqs_;

    set<string> stop_words_;

    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus_word = false;
        if (text[0] == '-') {
            is_minus_word = true;
            text = text.substr(1);
        }
        return {text, is_minus_word, IsStopWord(text)};
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            QueryWord query_word = ParseQueryWord(word);
            if (query_word.minus_word && !query_word.stop_word) {
                query.minus_words.insert(query_word.data);
            } else if (!query_word.stop_word) {
                query.plus_words.insert(query_word.data);
            }
        }
        return query;
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> matched_documents_map;
        for (const auto& word: query.plus_words) {
            if (word_to_document_freqs_.find(word) != word_to_document_freqs_.end()) {
                int size = word_to_document_freqs_.at(word).size();
                for (const auto [document_id, freq]: word_to_document_freqs_.at(word)) {
                    matched_documents_map[document_id] += log(1.0 * document_count_ / size) * freq;
                }
            }
        }
        for (const auto& word: query.minus_words) {
            if (word_to_document_freqs_.find(word) != word_to_document_freqs_.end()) {
                for (const auto [document_id, freq]: word_to_document_freqs_.at(word)) {
                    matched_documents_map.erase(document_id);
                }
            }
        }

        vector<Document> matched_documents;
        for (auto[id, relevance]: matched_documents_map) {
            matched_documents.push_back({id, relevance});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}

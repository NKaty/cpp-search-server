#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename Checker>
    vector<Document> FindTopDocuments(const string& raw_query, Checker checker) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, checker);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Checker>
    vector<Document> FindAllDocuments(const Query& query, Checker checker) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& data = documents_.at(document_id);
                if (checker(document_id, data.status, data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// Testing Library
template <typename T,typename U>
ostream& operator<<(ostream& os, const pair<T, U>& collection) {
    const auto& [key, value] = collection;
    os << key << ": "s << value;
    return os;
}

template <typename T>
void PrintCollection(ostream& os, const T& collection) {
    bool first = true;
    for (const auto& item: collection) {
        if (first) {
            os << item;
            first = false;
        } else {
            os << ", "s << item;
        }
    }
}

template <typename T, typename U>
ostream& operator<<(ostream& os, const map<T, U>& collection) {
    os << "{"s;
    PrintCollection(os, collection);
    os << "}"s;
    return os;
}

template <typename T>
ostream& operator<<(ostream& os, const set<T>& collection) {
    os << "{"s;
    PrintCollection(os, collection);
    os << "}"s;
    return os;
}

template <typename T>
ostream& operator<<(ostream& os, const vector<T>& collection) {
    os << "["s;
    PrintCollection(os, collection);
    os << "]"s;
    return os;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Tests
SearchServer GetSearchServerForTesting() {
    const int doc_id1 = 42;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const int doc_id2 = 1;
    const string content2 = "dog in the city"s;
    const vector<int> ratings2 = {3, 3, 3};
    const int doc_id3 = 3;
    const string content3 = "dog in the town"s;
    const vector<int> ratings3 = {100, 100, 100};
    const int doc_id4 = 10;
    const string content4 = "dog and cat"s;
    const vector<int> ratings4 = {-3, 3, 3};
    const int doc_id5 = 15;
    const string content5 = "dog and cat in the city"s;
    const vector<int> ratings5 = {1, 2, 3};
    const int doc_id6 = 29;
    const string content6 = "dog and cat in the town"s;
    const vector<int> ratings6 = {10, 20, 30};
    const int doc_id7 = 11;
    const string content7 = "dog and cat in the town in city"s;
    const vector<int> ratings7 = {0, 0, 0};
    const int doc_id8 = 19;
    const string content8 = "dog with cat in the town in city"s;
    const vector<int> ratings8 = {-10, 50, 1};

    SearchServer server;
    server.SetStopWords("in the and"s);
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id4, content4, DocumentStatus::REMOVED, ratings4);
    server.AddDocument(doc_id5, content5, DocumentStatus::ACTUAL, ratings5);
    server.AddDocument(doc_id6, content6, DocumentStatus::ACTUAL, ratings6);
    server.AddDocument(doc_id7, content7, DocumentStatus::ACTUAL, ratings7);
    server.AddDocument(doc_id8, content8, DocumentStatus::BANNED, ratings8);

    return server;
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestFindTopDocuments() {
    const SearchServer server = GetSearchServerForTesting();

    {
        const auto found_docs = server.FindTopDocuments("cat and dog"s);
        ASSERT_EQUAL(found_docs.size(), 5u);
        const Document &doc0 = found_docs[0];
        const Document &doc1 = found_docs[1];
        const Document &doc2 = found_docs[2];
        const Document &doc3 = found_docs[3];
        const Document &doc4 = found_docs[4];
        ASSERT_EQUAL(doc0.id, 42);
        ASSERT(doc0.relevance - 0.168236 < 1e-6);
        ASSERT_EQUAL(doc0.rating, 2);
        ASSERT_EQUAL(doc1.id, 29);
        ASSERT(doc1.relevance - 0.163541 < 1e-6);
        ASSERT_EQUAL(doc1.rating, 20);
        ASSERT_EQUAL(doc2.id, 15);
        ASSERT(doc2.relevance - 0.163541 < 1e-6);
        ASSERT_EQUAL(doc2.rating, 2);
        ASSERT_EQUAL(doc3.id, 11);
        ASSERT(doc3.relevance - 0.122656 < 1e-6);
        ASSERT_EQUAL(doc3.rating, 0);
        ASSERT_EQUAL(doc4.id, 3);
        ASSERT(doc4.relevance - 0.0770753 < 1e-6);
        ASSERT_EQUAL(doc4.rating, 100);
    }

    {
        const auto found_docs = server.FindTopDocuments("cat in city -town -dog"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Should check for minus words"s);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 42);
        ASSERT(doc0.relevance - 0.448044 < 1e-6);
        ASSERT_EQUAL(doc0.rating, 2);
    }
}

void TestFindTopDocumentsByLambda() {
    const SearchServer server = GetSearchServerForTesting();

    const auto found_docs = server.FindTopDocuments(
        "cat and dog"s,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
        });
    ASSERT_EQUAL(found_docs.size(), 2u);
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    ASSERT_EQUAL(doc0.id, 10);
    ASSERT(doc0.relevance - 0.245311 < 1e-6);
    ASSERT_EQUAL(doc0.rating, 1);
    ASSERT_EQUAL(doc1.id, 42);
    ASSERT(doc1.relevance - 0.168236 < 1e-6);
    ASSERT_EQUAL(doc1.rating, 2);
}
void TestFindTopDocumentsByStatus() {
    const SearchServer server = GetSearchServerForTesting();

    const auto found_docs = server.FindTopDocuments("cat and dog"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document& doc = found_docs[0];
    ASSERT_EQUAL(doc.id, 10);
    ASSERT(doc.relevance - 0.245311 < 1e-6);
    ASSERT_EQUAL(doc.rating, 1);
}

void TestGetDocumentCount() {
    const int doc_id1 = 42;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const int doc_id2 = 1;
    const string content2 = "dog in the city"s;
    const vector<int> ratings2 = {3, 3, 3};
    const int doc_id3 = 3;
    const string content3 = "dog in the town"s;
    const vector<int> ratings3 = {100, 100, 100};

    SearchServer server;
    server.SetStopWords("in the and"s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
    server.AddDocument(doc_id3, content3, DocumentStatus::REMOVED, ratings3);
    ASSERT_EQUAL(server.GetDocumentCount(), 3);
}

void TestMatchDocument() {
    const SearchServer server = GetSearchServerForTesting();

    {
        const vector<string> expected_words = {};
        const auto [words, status] = server.MatchDocument("dog"s, 42);
        ASSERT_EQUAL(words, expected_words);
        ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::ACTUAL));
    }

    {
        vector<string> expected_words = {"dog"s, "cat"s};
        sort(expected_words.begin(), expected_words.end());
        auto [words, status] = server.MatchDocument("dog without cat"s, 10);
        sort(words.begin(), words.end());
        ASSERT_EQUAL(words, expected_words);
        ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::REMOVED));
    }

    {
        const vector<string> expected_words = {};
        const auto [words, status] = server.MatchDocument("dog from the city -cat"s, 19);
        ASSERT_EQUAL(words, expected_words);
        ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED));
    }
}

void TestSplitIntoWords() {
    {
        const vector<string> expected_words = {};
        ASSERT_EQUAL(SplitIntoWords(""s), expected_words);
    }

    {
        const vector<string> expected_words = {"dog"s, "from"s, "the"s, "city"s};
        ASSERT_EQUAL(SplitIntoWords("dog from the city"s), expected_words);
    }

    {
        const vector<string> expected_words = {"dog"s, "from"s, "the"s, "city"s, "-no"s, "-cats"s};
        ASSERT_EQUAL(SplitIntoWords("dog from the city -no -cats"s), expected_words);
    }
}

// Launch tests
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestFindTopDocuments);
    RUN_TEST(TestFindTopDocumentsByLambda);
    RUN_TEST(TestFindTopDocumentsByStatus);
    RUN_TEST(TestGetDocumentCount);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSplitIntoWords);
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
    return 0;
}
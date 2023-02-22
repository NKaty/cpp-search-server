#include "paginator.h"
#include "request_queue.h"
//#include "remove_duplicates.h"

#include <iostream>
#include <string>

using namespace std;

// Testing Library
template<typename T, typename U>
ostream &operator<<(ostream &os, const pair<T, U> &collection) {
  const auto &[key, value] = collection;
  os << key << ": "s << value;
  return os;
}

template<typename T>
void PrintCollection(ostream &os, const T &collection) {
  bool first = true;
  for (const auto &item : collection) {
    if (first) {
      os << item;
      first = false;
    } else {
      os << ", "s << item;
    }
  }
}

template<typename T, typename U>
ostream &operator<<(ostream &os, const map<T, U> &collection) {
  os << "{"s;
  PrintCollection(os, collection);
  os << "}"s;
  return os;
}

template<typename T>
ostream &operator<<(ostream &os, const set<T> &collection) {
  os << "{"s;
  PrintCollection(os, collection);
  os << "}"s;
  return os;
}

template<typename T>
ostream &operator<<(ostream &os, const vector<T> &collection) {
  os << "["s;
  PrintCollection(os, collection);
  os << "]"s;
  return os;
}

template<typename T, typename U>
void AssertEqualImpl(const T &t,
                     const U &u,
                     const string &t_str,
                     const string &u_str,
                     const string &file,
                     const string &func,
                     unsigned line,
                     const string &hint) {
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

void AssertImpl(bool value,
                const string &expr_str,
                const string &file,
                const string &func,
                unsigned line,
                const string &hint) {
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

template<typename T>
void RunTestImpl(const T func, const string &func_name) {
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
  const string content6 = "dog and cat in the town cat"s;
  const vector<int> ratings6 = {10, 20, 30};
  const int doc_id7 = 11;
  const string content7 = "dog and cat in the town in city"s;
  const vector<int> ratings7 = {0, 0, 0};
  const int doc_id8 = 19;
  const string content8 = "dog with cat in the town in city"s;
  const vector<int> ratings8 = {-10, 50, 1};

  SearchServer server("in the and"s);
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
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document &doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
  }

  {
    SearchServer server("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                "Stop words must be excluded from documents"s);
  }
}

void TestFindTopDocuments() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments("cat and dog"s);
  ASSERT_EQUAL(found_docs.size(), 5u);
  ASSERT_EQUAL(found_docs[0].id, 29);
  ASSERT_EQUAL(found_docs[1].id, 42);
  ASSERT_EQUAL(found_docs[2].id, 15);
  ASSERT_EQUAL(found_docs[3].id, 11);
  ASSERT_EQUAL(found_docs[4].id, 3);
}

void TestFindTopDocumentsByLambda() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments(
      "cat and dog"s,
      [](int document_id, DocumentStatus status, int rating) {
        return document_id % 2 == 0;
      });
  ASSERT_EQUAL(found_docs.size(), 2u);
  ASSERT_EQUAL(found_docs[0].id, 10);
  ASSERT_EQUAL(found_docs[1].id, 42);
}

void TestFindTopDocumentsByStatus() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments("cat and dog"s, DocumentStatus::REMOVED);
  ASSERT_EQUAL(found_docs.size(), 1u);
  ASSERT_EQUAL(found_docs[0].id, 10);
}

void TestExcludeDocumentsWithMinusWordsFromFoundDocuments() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments("cat in city -town -dog"s);
  ASSERT_EQUAL(found_docs.size(), 1u);
  ASSERT_EQUAL_HINT(found_docs[0].id, 42, "Should exclude documents with minus words"s);
}

void TestComputeRelevance() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments("cat and dog"s);
  const double expected_relevance = log(8 * 1.0 / 6) * (2.0 / 4) + log(8 * 1.0 / 7) * (1.0 / 4);
  ASSERT_EQUAL(found_docs[0].id, 29);
  ASSERT_HINT(abs(found_docs[0].relevance - expected_relevance) < 1e-6,
              "Should compute relevance correctly"s);
}

void TestComputeAverageRating() {
  SearchServer server(""s);
  const vector<int> ratings = {-10, 50, 1};
  const int average_rating = (-10 + 50 + 1) / 3;
  server.AddDocument(10, "cat and dog"s, DocumentStatus::ACTUAL, ratings);
  const auto found_docs = server.FindTopDocuments("cat"s);
  ASSERT_EQUAL(found_docs[0].id, 10);
  ASSERT_EQUAL_HINT(found_docs[0].rating,
                    average_rating,
                    "Should compute average rating correctly"s);
}

void TestSortByRelevance() {
  const SearchServer server = GetSearchServerForTesting();

  const auto found_docs = server.FindTopDocuments("cat and dog"s);
  ASSERT_EQUAL(found_docs.size(), 5u);
  for (int i = 0; i < 4; ++i) {
    ASSERT_HINT(found_docs[i].relevance >= found_docs[i + 1].relevance,
                "Should sort by relevance correctly");
  }
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

  SearchServer server("in the and"s);
  ASSERT_EQUAL(server.GetDocumentCount(), 0);
  server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
  ASSERT_EQUAL(server.GetDocumentCount(), 1);
  server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
  ASSERT_EQUAL(server.GetDocumentCount(), 2);
  server.AddDocument(doc_id3, content3, DocumentStatus::REMOVED, ratings3);
  ASSERT_EQUAL(server.GetDocumentCount(), 3);
}

void TestMatchDocument1() {
  const SearchServer server = GetSearchServerForTesting();

  {
    const vector<string> expected_words = {};
    const auto [words, status] = server.MatchDocument("dog"s, 42);
    vector<string> actual_words(words.begin(), words.end());
    ASSERT_EQUAL(actual_words, expected_words);
    ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::ACTUAL));
  }

  {
    vector<string> expected_words = {"dog"s, "cat"s};
    sort(expected_words.begin(), expected_words.end());
    auto [words, status] =
        server.MatchDocument(execution::seq, "dog without cat"s, 10);
    sort(words.begin(), words.end());
    vector<string> actual_words(words.begin(), words.end());
    ASSERT_EQUAL(actual_words, expected_words);
    ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::REMOVED));
  }
}

void TestMatchDocumentWithMinusWords() {
  const SearchServer server = GetSearchServerForTesting();

  const vector<string> expected_words = {};
  const auto [words, status] = server.MatchDocument("dog from the city -cat"s, 19);
  ASSERT_EQUAL(words, vector<string_view>(expected_words.begin(), expected_words.end()));
  ASSERT_EQUAL(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED));
}

void TestSplitIntoWords() {
  {
    const vector<string> expected_words = {};
    ASSERT_EQUAL(SplitIntoWords(""s),
                 vector<string_view>(expected_words.begin(), expected_words.end()));
  }

  {
    const vector<string> expected_words = {"dog"s, "from"s, "the"s, "city"s};
    ASSERT_EQUAL(SplitIntoWords("dog from the city"s),
                 vector<string_view>(expected_words.begin(), expected_words.end()));
  }

  {
    const vector<string> expected_words = {"dog"s, "from"s, "the"s, "city"s, "-no"s, "-cats"s};
    auto words = SplitIntoWords("dog from the city -no -cats"s);
    ASSERT_EQUAL(SplitIntoWords("dog from the city -no -cats"s),
                 vector<string_view>(expected_words.begin(), expected_words.end()));
  }
}

// Launch tests
void TestSearchServer() {
  RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
  RUN_TEST(TestFindTopDocuments);
  RUN_TEST(TestFindTopDocumentsByLambda);
  RUN_TEST(TestFindTopDocumentsByStatus);
  RUN_TEST(TestGetDocumentCount);
  RUN_TEST(TestMatchDocument1);
  RUN_TEST(TestMatchDocumentWithMinusWords);
  RUN_TEST(TestSplitIntoWords);
  RUN_TEST(TestComputeRelevance);
  RUN_TEST(TestComputeAverageRating);
  RUN_TEST(TestSortByRelevance);
  RUN_TEST(TestExcludeDocumentsWithMinusWordsFromFoundDocuments);
}

void TestExamplePaginator() {
  SearchServer search_server("and with"s);
  search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
  search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
  search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
  search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
  search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});
  const auto search_results = search_server.FindTopDocuments("curly dog"s);
  int page_size = 2;
  const auto pages = Paginate(search_results, page_size);
  // Выводим найденные документы по страницам
  for (auto page = pages.begin(); page != pages.end(); ++page) {
    cout << *page << endl;
    cout << "Page break"s << endl;
  }
}

void TestExampleRequestQueue() {
  SearchServer search_server("and in at"s);
  RequestQueue request_queue(search_server);
  search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
  search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
  search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
  search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
  search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
  // 1439 запросов с нулевым результатом
  for (int i = 0; i < 1439; ++i) {
    request_queue.AddFindRequest("empty request"s);
  }
  // все еще 1439 запросов с нулевым результатом
  request_queue.AddFindRequest("curly dog"s);
  // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
  request_queue.AddFindRequest("big collar"s);
  // первый запрос удален, 1437 запросов с нулевым результатом
  request_queue.AddFindRequest("sparrow"s);
  cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;

}

//void TestExampleRemoveDuplicates() {
//  SearchServer search_server("and with"s);
//
//  search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
//  search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//  // дубликат документа 2, будет удалён
//  search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//  // отличие только в стоп-словах, считаем дубликатом
//  search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//  // множество слов такое же, считаем дубликатом документа 1
//  search_server.AddDocument(5,
//                            "funny funny pet and nasty nasty rat"s,
//                            DocumentStatus::ACTUAL,
//                            {1, 2});
//
//  // добавились новые слова, дубликатом не является
//  search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});
//
//  // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
//  search_server.AddDocument(7,
//                            "very nasty rat and not very funny pet"s,
//                            DocumentStatus::ACTUAL,
//                            {1, 2});
//
//  // есть не все слова, не является дубликатом
//  search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});
//
//  // слова из разных документов, не является дубликатом
//  search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
//
//  cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
//  RemoveDuplicates(search_server);
//  cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
//}

string GenerateWord(mt19937 &generator, int max_length) {
  const int length = uniform_int_distribution(1, max_length)(generator);
  string word;
  word.reserve(length);
  for (int i = 0; i < length; ++i) {
    word.push_back(uniform_int_distribution('a', 'z')(generator));
  }
  return word;
}

vector<string> GenerateDictionary(mt19937 &generator, int word_count, int max_length) {
  vector<string> words;
  words.reserve(word_count);
  for (int i = 0; i < word_count; ++i) {
    words.push_back(GenerateWord(generator, max_length));
  }
  sort(words.begin(), words.end());
  words.erase(unique(words.begin(), words.end()), words.end());
  return words;
}

string GenerateQuery(mt19937 &generator,
                     const vector<string> &dictionary,
                     int max_word_count,
                     double minus_prob = 0) {
  const int word_count = uniform_int_distribution(1, max_word_count)(generator);
  string query;
  for (int i = 0; i < word_count; ++i) {
    if (!query.empty()) {
      query.push_back(' ');
    }
    query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
  }
  return query;
}

vector<string> GenerateQueries(mt19937 &generator,
                               const vector<string> &dictionary,
                               int query_count,
                               int max_word_count) {
  vector<string> queries;
  queries.reserve(query_count);
  for (int i = 0; i < query_count; ++i) {
    queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
  }
  return queries;
}

template<typename ExecutionPolicy>
void TestRemoveDocumentWithPolicy(string_view mark,
                                  SearchServer search_server,
                                  ExecutionPolicy &&policy) {
  LOG_DURATION(mark);
  const int document_count = search_server.GetDocumentCount();
  for (int id = 0; id < document_count; ++id) {
    search_server.RemoveDocument(policy, id);
  }
  cout << search_server.GetDocumentCount() << endl;
}

void TestRemoveDocumentWithoutPolicy(string_view mark, SearchServer search_server) {
  LOG_DURATION(mark);
  const int document_count = search_server.GetDocumentCount();
  for (int id = 0; id < document_count; ++id) {
    search_server.RemoveDocument(id);
  }
  cout << search_server.GetDocumentCount() << endl;
}

#define TEST_REMOVE_DOCUMENT(mode) TestRemoveDocumentWithPolicy(#mode, search_server, execution::mode)

void TestRemoveDocument() {
  mt19937 generator;
  const auto dictionary = GenerateDictionary(generator, 10'000, 25);
  const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

  {
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
      search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST_REMOVE_DOCUMENT(seq);
  }
  {
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
      search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST_REMOVE_DOCUMENT(par);
  }

  {
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
      search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TestRemoveDocumentWithoutPolicy("plain", search_server);
  }
}

template<typename ExecutionPolicy>
void TestMatchDocumentWithPolicy(string_view mark,
                                 SearchServer search_server,
                                 const string &query,
                                 ExecutionPolicy &&policy) {
  LOG_DURATION(mark);
  const int document_count = search_server.GetDocumentCount();
  int word_count = 0;
  for (int id = 0; id < document_count; ++id) {
    const auto [words, status] = search_server.MatchDocument(policy, query, id);
    word_count += words.size();
  }
  cout << word_count << endl;
}

#define TEST_MATCH_DOCUMENT(policy) TestMatchDocumentWithPolicy(#policy, search_server, query, execution::policy)

void TestMatchDocument2() {
  mt19937 generator;
  const auto dictionary = GenerateDictionary(generator, 1000, 10);
  const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

  const string query = GenerateQuery(generator, dictionary, 500, 0.1);

  SearchServer search_server(dictionary[0]);
  for (size_t i = 0; i < documents.size(); ++i) {
    search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
  }

  TEST_MATCH_DOCUMENT(seq);
  TEST_MATCH_DOCUMENT(par);
}

template<typename ExecutionPolicy>
void TestFindTopDocumentsWithPolicy(string_view mark,
                                    const SearchServer &search_server,
                                    const vector<string> &queries,
                                    ExecutionPolicy &&policy) {
  LOG_DURATION(mark);
  double total_relevance = 0;
  for (const string_view query : queries) {
    for (const auto &document : search_server.FindTopDocuments(policy, query)) {
      total_relevance += document.relevance;
    }
  }
  cout << total_relevance << endl;
}
#define TEST_FIND_TOP_DOCUMENTS(policy) TestFindTopDocumentsWithPolicy(#policy, search_server, queries, execution::policy)

void TestFindTopDocuments2() {
  mt19937 generator;
  const auto dictionary = GenerateDictionary(generator, 1000, 10);
  const auto documents = GenerateQueries(generator, dictionary, 10000, 70);
  SearchServer search_server(dictionary[0]);
  for (size_t i = 0; i < documents.size(); ++i) {
    search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
  }
  const auto queries = GenerateQueries(generator, dictionary, 100, 70);
  TEST_FIND_TOP_DOCUMENTS(seq);
  TEST_FIND_TOP_DOCUMENTS(par);
}

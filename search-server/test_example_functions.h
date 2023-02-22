#pragma once

#include <iostream>
#include <string>

void TestExamplePaginator();

void TestExampleRequestQueue();

//void TestExampleRemoveDuplicates();

// Testing Library
template<typename T, typename U>
std::ostream &operator<<(std::ostream &os, const std::pair<T, U> &collection);

template<typename T>
void PrintCollection(std::ostream &os, const T &collection);

template<typename T, typename U>
std::ostream &operator<<(std::ostream &os, const std::map<T, U> &collection);

template<typename T>
std::ostream &operator<<(std::ostream &os, const std::set<T> &collection);

template<typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &collection);

template<typename T, typename U>
void AssertEqualImpl(const T &t,
                     const U &u,
                     const std::string &t_str,
                     const std::string &u_str,
                     const std::string &file,
                     const std::string &func,
                     unsigned line,
                     const std::string &hint);

void AssertImpl(bool value,
                const std::string &expr_str,
                const std::string &file,
                const std::string &func,
                unsigned line,
                const std::string &hint);

template<typename T>
void RunTestImpl(const T func, const std::string &func_name);

// Tests
SearchServer GetSearchServerForTesting();

void TestExcludeStopWordsFromAddedDocumentContent();

void TestFindTopDocuments();

void TestFindTopDocumentsByLambda();

void TestFindTopDocumentsByStatus();

void TestExcludeDocumentsWithMinusWordsFromFoundDocuments();

void TestComputeRelevance();

void TestComputeAverageRating();

void TestSortByRelevance();

void TestGetDocumentCount();

void TestMatchDocument1();

void TestMatchDocumentWithMinusWords();

void TestSplitIntoWords();

// Launch tests
void TestSearchServer();

std::string GenerateWord(std::mt19937 &generator, int max_length);

std::vector<std::string> GenerateDictionary(std::mt19937 &generator,
                                            int word_count,
                                            int max_length);

std::string GenerateQuery(std::mt19937 &generator,
                          const std::vector<std::string> &dictionary,
                          int max_word_count,
                          double minus_prob = 0);

std::vector<std::string> GenerateQueries(std::mt19937 &generator,
                                         const std::vector<std::string> &dictionary,
                                         int query_count,
                                         int max_word_count);

template<typename ExecutionPolicy>
void TestRemoveDocumentWithPolicy(std::string_view mark,
                                  SearchServer search_server,
                                  ExecutionPolicy &&policy);

void TestRemoveDocumentWithoutPolicy(std::string_view mark, SearchServer search_server);

void TestRemoveDocument();

template<typename ExecutionPolicy>
void TestMatchDocumentWithPolicy(std::string_view mark,
                                 SearchServer search_server,
                                 const std::string &query,
                                 ExecutionPolicy &&policy);

void TestMatchDocument2();

template<typename ExecutionPolicy>
void TestFindTopDocumentsWithPolicy(std::string_view mark,
                                    const SearchServer &search_server,
                                    const std::vector<std::string> &queries,
                                    ExecutionPolicy &&policy);
void TestFindTopDocuments2();

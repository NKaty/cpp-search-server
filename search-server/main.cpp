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

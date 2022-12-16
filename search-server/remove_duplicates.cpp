#include "remove_duplicates.h"

#include <set>
#include <vector>
#include <string>
#include <iostream>

using namespace std;

void RemoveDuplicates(SearchServer &search_server) {
  vector<int> ids_to_remove;
  set<set<string>> unique_words;
  for (const int document_id : search_server) {
    set<string> words;
    for (const auto &[word, _] : search_server.GetWordFrequencies(document_id)) {
      words.insert(word);
    }
    if (unique_words.find(words) != unique_words.end()) {
      ids_to_remove.push_back(document_id);
      cout << "Found duplicate document id "s << document_id << "\n"s;
    } else {
      unique_words.insert(words);
    }
  }
  for (const auto id : ids_to_remove) {
    search_server.RemoveDocument(id);
  }
}

#include "process_queries.h"

#include <string>
#include <vector>
#include <list>
#include <execution>
#include <numeric>

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer &search_server,
    const vector<string> &queries) {
  vector<vector<Document>> result(queries.size());
  transform(
      execution::par,
      queries.begin(),
      queries.end(),
      result.begin(),
      [&search_server](const auto &query) {
        return search_server.FindTopDocuments(query);
      }
  );
  return result;
}

list<Document> ProcessQueriesJoined(
    const SearchServer &search_server,
    const vector<string> &queries) {
  list<Document> result;
  for (const auto &collection : ProcessQueries(search_server, queries)) {
    result.insert(result.end(), collection.begin(), collection.end());
  }
  return result;
}

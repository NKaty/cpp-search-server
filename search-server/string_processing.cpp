#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
  vector<string_view> words;
  if (text.empty()) {
    return words;
  }
  for (size_t start = 0; start <= text.size();) {
    size_t end = text.find(' ', start);
    if (start != end) {
      words.push_back(text.substr(start, end - start));
    }
    start = end == string_view::npos ? end : end + 1;
  }
  return words;
}

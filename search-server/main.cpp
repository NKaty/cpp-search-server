#include "paginator.h"
#include "request_queue.h"
#include "test_example_functions.h"
#include "process_queries.h"

int main() {
  TestExamplePaginator();
  TestExampleRequestQueue();
//  TestExampleRemoveDuplicates();
  TestSearchServer();
  TestRemoveDocument();
  TestMatchDocument2();
  TestFindTopDocuments2();
  return 0;
}

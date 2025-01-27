#include "stdafx.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

using namespace std;


// Type definitions.
// Directed graph.
typedef PNGraph PGraph;


// Represents an edge.
struct Edge {
  int u, v;
  Edge(int u, int v) : u(u), v(v) {}
};




// Get current time.
inline clock_t timeNow() {
  return clock();
}


// Get time duration, in milliseconds.
inline float duration(clock_t start, clock_t end) {
  return 1000.0 * (end - start) / CLOCKS_PER_SEC;
}


// Get random integer in the range [begin, end].
inline int randomInt(int begin, int end) {
  unsigned int r0 = rand() & 0xFFFF;
  unsigned int r1 = rand() & 0xFFFF;
  unsigned int r = (r0 << 16) | r1;
  return begin + r % (end + 1 - begin);
}


// Generate edge deletions.
inline vector<Edge> generateEdgeDeletions(PGraph x, int batchSize, bool isSymmetric) {
  int retries = 5;
  int n = x->GetNodes();
  vector<Edge> deletions;
  for (int b=0; b<batchSize; ++b) {
    for (int r=0; r<retries; ++r) {
      int u = randomInt(1, n);
      TNGraph::TNodeI it = x->GetNI(u);
      int degree = it.GetOutDeg();
      if (degree == 0) continue;
      int j = randomInt(0, degree-1);
      int v = it.GetOutNId(j);
      deletions.push_back({u, v});
      if (isSymmetric) deletions.push_back({v, u});
      break;
    }
  }
  return deletions;
}


// Generate edge insertions.
inline vector<Edge> generateEdgeInsertions(PGraph x, int batchSize, bool isSymmetric) {
  int retries = 5;
  int n = x->GetNodes();
  vector<Edge> insertions;
  for (int b=0; b<batchSize; ++b) {
    for (int r=0; r<retries; ++r) {
      int u = randomInt(1, n);
      int v = randomInt(1, n);
      if (x->IsEdge(u, v)) continue;
      insertions.push_back({u, v});
      if (isSymmetric) insertions.push_back({v, u});
      break;
    }
  }
  return insertions;
}


// Main function.
int main(int argc, char* argv[]) {
  char *file = argv[1];
  bool symmetric = argc>2? atoi(argv[2]) : false;
  bool weighted  = argc>3? atoi(argv[3]) : false;
  int  rows      = argc>4? atoi(argv[4]) : 0;
  int  size      = argc>5? atoi(argv[5]) : 0;
  // Load graph from Edgelist file.
  printf("Loading graph %s [nodes=%d, edges=%d] ...\n", file, rows, size);
  clock_t t0 = timeNow();
  PGraph x = TSnap::LoadEdgeList<PGraph>(file, 0, 1);
  for (int i=1; i<=rows; ++i) {
    if (x->IsNode(i)) continue;
    x->AddNodeUnchecked(i);
  }
  clock_t t1 = timeNow();
  printf("Nodes: %d, Edges: %d\n", x->GetNodes(), x->GetEdges());
  printf("Elapsed time: %.2f ms\n", duration(t0, t1));
  printf("\n");
  // Perform batch updates of varying sizes.
  for (int batchPower=-7; batchPower<=-1; ++batchPower) {
    int m = x->GetEdges();
    double batchFraction = pow(10, batchPower);
    int batchSize = int(round(batchFraction * m));
    printf("Batch fraction: %.1e [%d edges]\n", batchFraction, batchSize);
    // Perform edge deletions.
    {
      vector<Edge> deletions = generateEdgeDeletions(x, batchSize, symmetric);
      printf("Cloning graph ...\n");
      clock_t t0 = timeNow();
      PGraph y = TSnap::ConvertGraph<PGraph>(x);
      clock_t t1 = timeNow();
      printf("Nodes: %d, Edges: %d\n", y->GetNodes(), y->GetEdges());
      printf("Elapsed time: %.2f ms\n", duration(t0, t1));
      printf("Deleting edges [%zu edges] ...\n", deletions.size());
      clock_t t2 = timeNow();
      for (int i=0; i<deletions.size(); ++i) {
        Edge e = deletions[i];
        y->DelEdge(e.u, e.v);
      }
      clock_t t3 = timeNow();
      printf("Nodes: %d, Edges: %d\n", y->GetNodes(), y->GetEdges());
      printf("Elapsed time: %.2f ms\n", duration(t2, t3));
      for (int i=0; i<deletions.size(); ++i) {
        Edge e = deletions[i];
        Assert(!y->IsEdge(e.u, e.v));
      }
    }
    // Perform edge insertions.
    {
      vector<Edge> insertions = generateEdgeInsertions(x, batchSize, symmetric);
      printf("Cloning graph ...\n");
      clock_t t0 = timeNow();
      PGraph y = TSnap::ConvertGraph<PGraph>(x);
      clock_t t1 = timeNow();
      printf("Nodes: %d, Edges: %d\n", y->GetNodes(), y->GetEdges());
      printf("Elapsed time: %.2f ms\n", duration(t0, t1));
      printf("Inserting edges [%zu edges] ...\n", insertions.size());
      clock_t t2 = timeNow();
      for (int i=0; i<insertions.size(); ++i) {
        Edge e = insertions[i];
        y->AddEdge(e.u, e.v);
      }
      clock_t t3 = timeNow();
      printf("Nodes: %d, Edges: %d\n", y->GetNodes(), y->GetEdges());
      printf("Elapsed time: %.2f ms\n", duration(t2, t3));
      for (int i=0; i<insertions.size(); ++i) {
        Edge e = insertions[i];
        Assert(y->IsEdge(e.u, e.v));
      }
    }
    printf("\n");
  }
  printf("\n");
  return 0;
}

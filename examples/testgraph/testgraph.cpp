#include "stdafx.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <iterator>

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


// Copy graph.
inline PGraph copy_graph(PGraph x) {
  PGraph y  = TNGraph::New();
  TNGraph::TNodeI vb = x->BegNI(), ve = x->EndNI();
  TNGraph::TEdgeI eb = x->BegEI(), ee = x->EndEI();
  for (TNGraph::TNodeI it=vb; it<ve; it++)
    y->AddNode(it.GetId());
  for (TNGraph::TEdgeI it=eb; it<ee; it++)
    y->AddEdge(it.GetSrcNId(), it.GetDstNId());
  return y;
}


// Load graph.
inline PGraph load_graph(const char *file) {
  PGraph x = TSnap::LoadEdgeList<PGraph>(file, 0, 1);
  for (int i=1; i<=x->GetNodes(); ++i) {
    if (x->IsNode(i)) continue;
    x->AddNodeUnchecked(i);
  }
  return x;
}


// Test transpose graph.
inline void test_transpose_graph(PGraph x) {
  PGraph y = TNGraph::New();
  printf("Transposing graph ...\n");
  clock_t t0 = timeNow();
  TNGraph::TNodeI vb = x->BegNI(), ve = x->EndNI();
  TNGraph::TEdgeI eb = x->BegEI(), ee = x->EndEI();
  for (TNGraph::TNodeI it=vb; it<ve; it++)
    y->AddNode(it.GetId());
  for (TNGraph::TEdgeI it=eb; it<ee; it++)
    y->AddEdge(it.GetDstNId(), it.GetSrcNId());
  clock_t t1 = timeNow();
  printf("Nodes: %d, Edges: %d\n", y->GetNodes(), y->GetEdges());
  printf("Elapsed time: %.2f ms\n", duration(t0, t1));
  y->Clr();
}


// Test multi-step visit count with BFS, from all vertices.
inline void test_visit_count_bfs(PGraph x, int steps) {
  printf("Testing multi-step visit count with BFS ...\n");
  int n = x->GetNodes();
  int m = x->GetEdges();
  vector<size_t> visits0(n, 1);
  vector<size_t> visits1(n, 0);
  clock_t t0 = timeNow();
  for (int s=0; s<steps; ++s) {
    #pragma omp parallel for schedule(dynamic, 512)
    for (int u=0; u<n; ++u) {
      if (!x->IsNode(u)) continue;
      visits0[u] = 0;
      TNGraph::TNodeI vb = x->GetNI(u);
      int d = vb.GetOutDeg();
      for (int j=0; j<d; ++j) {
        int v = vb.GetOutNId(j);
        visits1[u] += visits0[v];
      }
    }
    swap(visits0, visits1);
  }
  clock_t t1 = timeNow();
  size_t total = 0;
  for (int i=0; i<n; ++i)
    total += visits0[i];
  printf("Total visits: %zu\n", total);
  printf("Elapsed time: %.2f ms\n", duration(t0, t1));
}


// Main function.
int main(int argc, char* argv[]) {
  char *file = argv[1];
  bool symmetric = argc>2? atoi(argv[2]) : false;
  bool weighted  = argc>3? atoi(argv[3]) : false;
  int  rows      = argc>4? atoi(argv[4]) : 0;
  int  size      = argc>5? atoi(argv[5]) : 0;
  int  steps     = argc>6? atoi(argv[6]) : 42;
  // Load graph from Edgelist file.
  printf("Loading graph %s [nodes=%d, edges=%d] ...\n", file, rows, size);
  clock_t t0 = timeNow();
  PGraph   x = load_graph(file);
  clock_t t1 = timeNow();
  printf("Nodes: %d, Edges: %d\n", x->GetNodes(), x->GetEdges());
  printf("Elapsed time: %.2f ms\n", duration(t0, t1));
  test_transpose_graph(x);
  int n = x->GetNodes();
  int m = x->GetEdges();
  x->Clr();
  printf("\n");
  // Perform batch updates of varying sizes.
  for (int batchPower=-7; batchPower<=-1; ++batchPower) {
    double batchFraction = pow(10, batchPower);
    int batchSize = int(round(batchFraction * m));
    printf("Batch fraction: %.1e [%d edges]\n", batchFraction, batchSize);
    // Perform edge deletions.
    {
      PGraph y = load_graph(file);  // Reloading graph is faster than copying
      vector<Edge> deletions = generateEdgeDeletions(y, batchSize, symmetric);
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
      test_visit_count_bfs(y, steps);
      y->Clr();
    }
    // Perform edge insertions.
    {
      PGraph y = load_graph(file);  // Reloading graph is faster than copying
      vector<Edge> insertions = generateEdgeInsertions(y, batchSize, symmetric);
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
      test_visit_count_bfs(y, steps);
      y->Clr();
    }
    printf("\n");
  }
  printf("\n");
  return 0;
}

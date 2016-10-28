/// My typedefs
#include <inttypes.h>
#include <boost/cstdint.hpp>
#include <boost/integer_traits.hpp>

typedef int32_t    node_t;
typedef int32_t    edge_t;
typedef int64_t    cost_t;

/// From STL library
#include <fstream>

#include <vector>
using std::vector;

#include <string>

using std::pair;
using std::make_pair;


/// Lemon Graph Library
#include <lemon/list_graph.h>
#include <lemon/smart_graph.h>
#include <lemon/static_graph.h>
#include <lemon/fib_heap.h>
#include <lemon/quad_heap.h>
#include <lemon/radix_heap.h>
#include <lemon/dheap.h>
#include <lemon/bin_heap.h>

// Johannes Sommer
// change graph and heap type at compile time
// Don't optimize too much
// -O1 gives better results on static graphs

// choose ONE graph type
//#define USE_LISTGRAPH
//#define USE_SMARTGRAPH
#define USE_STATICGRAPH

// choose ONE heap type

//#define USE_BIN_HEAP
//#define USE_QUAD_HEAP
//#define USE_FIBONACCI_HEAP
//#define USE_RADIX_HEAP
//#define USE_DARY_HEAP_D_2
#define USE_DARY_HEAP_D_3
//#define USE_DARY_HEAP_D_4
//#define USE_DARY_HEAP_D_8
//#define USE_DARY_HEAP_D_16


#ifdef USE_LISTGRAPH
typedef lemon::ListDigraph graph_t;
#endif
#ifdef USE_SMARTGRAPH
typedef lemon::SmartDigraph graph_t;
#endif
#ifdef USE_STATICGRAPH
typedef lemon::StaticDigraph graph_t;
#endif

#include <lemon/adaptors.h>
#include <lemon/concepts/maps.h>
#include <lemon/dijkstra.h>
#include <lemon/path.h>
typedef graph_t::Arc  Arc;
typedef graph_t::Node Node;
typedef graph_t::ArcMap<cost_t>   LengthMap;
typedef graph_t::NodeMap<int>  NodeMap;

#ifdef USE_FIBONACCI_HEAP
typedef lemon::FibHeap<cost_t, NodeMap> heap_t;
#endif
#ifdef USE_QUAD_HEAP
typedef lemon::QuadHeap<cost_t, NodeMap> heap_t;
#endif
#ifdef USE_RADIX_HEAP
typedef lemon::RadixHeap<NodeMap> heap_t;
#endif
#ifdef USE_DARY_HEAP_D_2
typedef lemon::DHeap<cost_t, NodeMap, 2> heap_t;
#endif
#ifdef USE_DARY_HEAP_D_3
typedef lemon::DHeap<cost_t, NodeMap, 3> heap_t;
#endif
#ifdef USE_DARY_HEAP_D_4
typedef lemon::DHeap<cost_t, NodeMap, 4> heap_t;
#endif
#ifdef USE_DARY_HEAP_D_8
typedef lemon::DHeap<cost_t, NodeMap, 8> heap_t;
#endif
#ifdef USE_DARY_HEAP_D_16
typedef lemon::DHeap<cost_t, NodeMap, 16> heap_t;
#endif

#ifdef USE_BIN_HEAP
// default heap type
typedef lemon::BinHeap<cost_t, NodeMap> heap_t;
#endif

/// Boost Timer
#include <boost/progress.hpp>
using boost::timer;

using namespace boost;

/// Read input data, build graph, and run Dijkstra
cost_t runDijkstra( char* argv[] ) {
   /// Read instance from the OR-lib
   std::ifstream infile(argv[1]);
   if (!infile)
      exit ( EXIT_FAILURE );

   int n;     /// Number of variables
   int m;     /// Number of constraints

   // reads file of the form
   // #nodes #edges
   // e_1 = v_i v_j cost[e_m]
   // ..
   // e_m = v_i v_j cost[e_m]

   /// Read the first line
   infile >> n >> m;
   fprintf(stdout,"n %d, m %d\t", n, m);

   /// Build the graph
   graph_t G;

#ifndef USE_STATICGRAPH
   G.reserveNode(n);
   G.reserveArc(m);
   vector<Node> vs;
   vs.reserve(n);
   for ( int i = 0; i < n; ++i )
      vs.push_back( G.addNode() );

   int v, w;
   cost_t c;
   cost_t T_dist;
   LengthMap    C(G);
   for ( int i = 0; i < m; i++ ) {
      infile >> v >> w >> c;
      Arc a;
      a = G.addArc(vs[v-1], vs[w-1]);
      C[a] = c;
   }
#else
    vector<int> vs;
    vs.reserve(n);
    for ( int i = 0; i < n; ++i )
      vs.push_back( i ); //lemon ids start from 1

    int v, w;
    cost_t c;
    cost_t T_dist;
    LengthMap    C(G);
    //arcs will be built with arcList iterators
    // populate arcList first
    vector< pair<int,int> > arcList (m);
    vector<cost_t> costs (m);
    for ( int i = 0; i < m; i++ ) {
      infile >> v >> w >> c;
      costs.push_back(c);
      arcList.push_back(make_pair(vs[v-1],vs[w-1]));
    }
    G.build(vs.size(), arcList.begin(), arcList.end());

    size_t s = arcList.size();

    //free ressources immediately
    vector< pair<int,int> > ().swap(arcList);

    //populate costMap
    for (int i = 0; i < s; i++)
        C[ G.arc(i) ] = costs[i];

    //free ressources immediately
    vector<cost_t> ().swap(costs);

#endif // USE_STATICGRAPH

   timer TIMER;
   for ( int i = 0; i < 10; ++i ) {
      double t0 = TIMER.elapsed();

#ifndef USE_STATICGRAPH
      Node S = vs[i];
      Node T = vs[n-1-i];
#else
      Node S = G.nodeFromId(vs[i]);
      Node T = G.nodeFromId(vs[n-1-i]);
#endif // USE_STATICGRAPH

      //heap change
      NodeMap heap_cross_ref(G);
      heap_t heap(heap_cross_ref);
      lemon::Dijkstra<graph_t, LengthMap>::SetHeap<heap_t, NodeMap>::Create spp(G, C);
      spp.heap( heap, heap_cross_ref );
      //else
      //lemon::Dijkstra<graph_t, LengthMap> spp(G, C);
      spp.run(S,T);
      T_dist = spp.dist(T);
      fprintf(stdout,"Time %.4f Cost %"PRId64"\n", TIMER.elapsed()-t0, T_dist);
   }
   fprintf(stdout,"Tot %.4f\n", TIMER.elapsed());

   return T_dist;
}

/// Main function
int
main (int argc, char **argv)
{
   if ( argc != 2 ) {
      fprintf(stdout, "usage: ./dijkstra <filename>\n");
      exit ( EXIT_FAILURE );
   }
   /// Measure overall time
   timer TIMER;
   /// Invoke the different Dijkstra algorithm implementations
   cost_t T_dist = runDijkstra(argv);
   /// Print basic figures
   fprintf(stdout,"Cost %"PRId64" - Time %.3f\n", T_dist, TIMER.elapsed());

   return 0;
}

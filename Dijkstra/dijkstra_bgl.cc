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

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/dijkstra_shortest_paths_no_color_map.hpp>

using namespace boost;

// Johannes Sommer
// change graph type at compile time
//#define USE_ADJACENCY_LIST
#define USE_COMPRESSED_SPARSE_ROW_GRAPH

// change algorithm at compile time
//#define USE_DIJKSTRA
#define USE_DIJKSTRA_NO_COLOR_MAP

#ifdef USE_COMPRESSED_SPARSE_ROW_GRAPH
typedef compressed_sparse_row_graph < directedS, no_property, property<edge_weight_t, cost_t> >   Digraph;
#endif
#ifdef USE_ADJACENCY_LIST
typedef adjacency_list < vecS, vecS, directedS, no_property, property<edge_weight_t, cost_t> >   Digraph;
#endif

typedef graph_traits<Digraph>::vertex_descriptor  Node;
typedef graph_traits<Digraph>::edge_descriptor    Arc;

/// Boost Timer
#include <boost/progress.hpp>
using boost::timer;

using namespace boost;

//defines for heap in dijkstra
//#define BOOST_GRAPH_DIJKSTRA_TESTING
//#define BOOST_GRAPH_DIJKSTRA_USE_RELAXED_HEAP
//else: use d-ary heap (d = 4)

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
   fprintf(stdout,"n %d, m %d\n", n, m);

   /// Build the graph
   int v, w;
   cost_t c;

#ifdef USE_COMPRESSED_SPARSE_ROW_GRAPH

   vector< pair<int,int> > edges(m);
   vector<cost_t> costs (m);
   for ( int i = 0; i < m; i++ ) {
      infile >> v >> w >> c;
      edges.push_back(make_pair(v-1,w-1));
      costs.push_back(c);
   }
   Digraph G(boost::edges_are_unsorted, edges.begin(), edges.end(), &costs[0], n);

   //free immediately
   vector< pair<int,int> > ().swap(edges);
   vector< cost_t > ().swap(costs);

#else
   /// Build the graph
   Digraph G (n);

   for ( int i = 0; i < m; i++ ) {
      infile >> v >> w >> c;
      add_edge(v-1, w-1, c, G);
   }

#endif // USE_COMPRESSED_SPARSE_ROW_GRAPH

   vector<Node>    P(n);
   vector<cost_t>  D(n,std::numeric_limits<cost_t>::max());
   cost_t T_dist;

   timer TIMER;
   for ( int i = 0; i < 10; ++i ) {
      double t0 = TIMER.elapsed();
      node_t S = i;
      node_t T = n-1-i;
#ifdef USE_DIJKSTRA_NO_COLOR_MAP
    dijkstra_shortest_paths_no_color_map(G, S, predecessor_map(&P[0]).distance_map(&D[0]) );
#endif
#ifdef USE_DIJKSTRA
    dijkstra_shortest_paths(G, S, predecessor_map(&P[0]).distance_map(&D[0]));
#endif

      T_dist = D[T];
      fprintf(stdout,"Time %.4f Cost %"PRId64"\n", TIMER.elapsed()-t0, T_dist);
   }
   fprintf(stdout,"Tot %.4f\n", TIMER.elapsed());

   return T_dist;
}

///------------------------------------------------------------------------------------------
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
   fprintf(stdout,"Cost %" PRId64 " - Time %.3f\n", T_dist, TIMER.elapsed());

   return 0;
}


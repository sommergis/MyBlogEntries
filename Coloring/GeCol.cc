/*
 *  Main authors:
 *     Stefano Gualandi <stefano.gualandi@gmail.com>
 *
 *  Copyright:
 *     Stefano Gualandi, 2010
 *
 *  Last update: May, 4th, 2013
 */

/// Limits on numbers
#include <limits>
using std::numeric_limits;
#include <vector>
using std::vector;

#include <gecode/driver.hh>


/// Use Cliquer to find all maximal clique in the conflict graph
extern "C" {
#include "cliquer.h"
#include "set.h"
#include "graph.h"
#include "reorder.h"
}

using namespace Gecode;
using namespace Gecode::Int;

/// Cutoff object for the script
class MyCutoff : public Search::Stop {
   private:
      Search::TimeStop*    ts; 
      Search::MemoryStop*  ms; 

      MyCutoff(int time, int memory)
         : ts((time > 0)   ? new Search::TimeStop(time) : NULL),
         ms((memory > 0) ? new Search::MemoryStop(memory) : NULL) {}
   public:
      virtual bool stop(const Search::Statistics& s, const Search::Options& o) {
         return
            ((ts != NULL) && ts->stop(s,o)) ||
            ((ms != NULL) && ms->stop(s,o));
      }
      virtual bool stopTime(const Search::Statistics& s, const Search::Options& o) {
         return
            ((ts != NULL) && ts->stop(s,o));
      }
      virtual bool stopMemory(const Search::Statistics& s, const Search::Options& o) {
         return
            ((ms != NULL) && ms->stop(s,o));
      }
      static Search::Stop*
         create(int time, int memory) {
            if ((time == 0) && (memory == 0))
               return NULL;
            else
               return new MyCutoff(time,memory);
         }
      ~MyCutoff(void) {
         delete ts; delete ms;
      }
};

/// Main Script
class GraphColoring : public Script {
   protected:
      /// Mapping vertices to colors
      IntVarArray   x;  
   public:
      /// Actual model
      GraphColoring( const graph_t*  g, int k, 
            const set_t* C, int n_c, const set_t* Cs, bool saveMemory = false ) 
         :  x ( *this, g->n, 1, k )     /// Colors start from '1'                  
      {  
         int n = g->n;
         /// Assign "colors" to the vertices of the max-clique
         int col = 1;
         for ( int i = 0; i < n; ++i )
            if ( SET_CONTAINS_FAST(C[0],i) )
               rel ( *this, x[i], IRT_EQ, col++);

         /// Post an 'alldifferent' constraints for each maximal cliques
         for ( int i = 0; i < n_c; ++i ) {
            IntVarArgs xdiff( set_size(Cs[i]) );
            int idx = 0;
            for ( int j = 0; j < n; ++j ) 
               if ( SET_CONTAINS_FAST(Cs[i],j) ) {
                  xdiff[idx] = x[j];
                  idx++;
               }

            distinct ( *this, xdiff, ICL_DOM );       
         }

         /// Post constraints on edges
         for ( int i = 0; i < n; ++i )
            for ( int j = i+1; j < n ; ++j )
               if ( GRAPH_IS_EDGE(g,i,j) )
                     rel ( *this, x[i], IRT_NQ, x[j] );
         /// Symmetry breaking
         Symmetries syms;
         syms << ValueSymmetry(IntArgs::create(k,1));
         //MySymBranch<Int::IntView>::post ( *this, x, g );
         //branch(*this, x, INT_VAR_AFC_SIZE_MAX(), INT_VAL_MIN(), syms);
         //branch(*this, x, INT_VAR_ACTIVITY_SIZE_MAX(), INT_VAL_MIN());
         branch(*this, x, INT_VAR_DEGREE_SIZE_MIN(), INT_VAL_MIN(),syms);
      }

      GraphColoring( bool share, GraphColoring& s) : Script(share,s) {
         x.update ( *this, share, s.x );
      }

      /// Perform copying during cloning
      virtual Space*
         copy(bool share) {
            return new GraphColoring(share, *this);
         }

      virtual int
         getChi(void) {
            int k = 0;
            for ( int i = x.size(); i--; )
               k = std::max( k, x[i].val() );
            return k;
         }

      virtual vector< vector<int> >
         getColors(void) {
            vector< vector<int> > colors;
            for ( int i = x.size(); i--; ) {
               vector<int> color;
               colors.push_back(color);
            }
            for ( int i = x.size(); i--; )
               colors[x[i].val()].push_back( i );

            return colors;
         }
};



/// Find upper bounds to coloring
int
colorHeuristic ( const graph_t*    g, 
      const set_t*    maxClique,
      int             n_c,        /// Number of cliques in "cliques"
      const set_t*    cliques,
      int             UB,
      double          time = 3600,
      int             memory = numeric_limits<int>::max(),
      int             n_threads = 1,
      bool            saveMemory = false
      )
{
   /// Solution of the problem
   Support::Timer t;
   t.start();

   double elapsed;
   int tot_nodes = 0;

   Search::Options so;
   so.threads = n_threads;
   so.clone   = false;

   do {
      UB--; /// Find a coloring of better cost

      GraphColoring* s = new GraphColoring ( g, UB, maxClique, n_c, cliques, saveMemory );
      elapsed = time - t.stop();

      if ( elapsed <= 0.001 ) {
         fprintf(stdout,"\ttimeout of CP solver elapsed\n");
         break;
      }

      so.stop = MyCutoff::create( elapsed, memory );

      DFS<GraphColoring> e(s, so);
      int scriptMemory = e.statistics().memory;

      GraphColoring* ex = e.next();
      if ( e.stopped() ) {
         fprintf(stdout,"\tWARNING: STOPPED, IT IS ONLY AN UPPER BOUND!\n");

         MyCutoff* myc = dynamic_cast<MyCutoff *>(so.stop);
         if ( myc->stopTime(e.statistics(),so) )
            fprintf(stdout," TIME LIMIT!\n");
         if ( myc->stopMemory(e.statistics(),so) )
            fprintf(stdout," MEMORY LIMIT!\n");
      }

      tot_nodes += e.statistics().node;

      if ( ex == NULL ) {
         UB++;
         break;
      }
      UB = std::min(UB,ex->getChi());
      fprintf(stdout,"\tNew upper bound: %d - Memory used by current model: %d - Nodes visit by current model: %ld\n",
            UB, scriptMemory, e.statistics().node);

      delete ex;
   } while ( time - t.stop() >= 0.001 );

   fprintf(stdout, "Number of nodes = %d - Time %.3f", tot_nodes, t.stop());

   return UB;
}





/// MAIN PROGRAM
int main(int argc, char **argv)
{
   if ( argc != 2 ) {
      fprintf(stdout, "\nusage:  $ ./GeCol <filename>\n\n");
      exit(EXIT_SUCCESS);
   }

   int  timeout = 300*1000;  /// 5 minutes timeout
   int  memoryLimit = numeric_limits<int>::max();
   int  threads = 1;
   bool saveMemory = false;
   
   clique_options* opts;
   graph_t* g;  
   set_t s;
   set_t  C = NULL;
   int    n_c = 0;   /// Number of maximal cliques found
   set_t* Cs;    /// Collection of maximal cliques
   int* table;
   int LB = 0;
   int UB;
   int n;

   /// Set the options for using Cliquer
   opts = (clique_options*) malloc (sizeof(clique_options));
   opts->time_function=NULL;
   opts->reorder_function=reorder_by_greedy_coloring;
   opts->reorder_map=NULL;
   opts->user_function=NULL;
   opts->user_data=NULL;
   opts->clique_list=NULL;
   opts->clique_list_length=0;


   /// Read a graph instance in any DIMACS format (binary or ascii)
   g = graph_read_dimacs_file(argv[1]);
   n = g->n;
   UB = n;
   
   /// Allocate space to store maximal cliques
   Cs = (set_t*)malloc(n*sizeof(set_t));
   C  = set_new(n);

   /// Use Cliquer implementation of greedy coloring heuristic
   table = reorder_by_greedy_coloring(g,FALSE);
   { /// Copmute the number of colors used
      int k = 0;
      for ( int i = 0; i < n; ++i )
         k = std::max(k,table[i]);
      UB = std::min(UB,k);
   }

   /// Find a maximal clique for every vertex, and store the largest
   for ( int i = 0; i < n; ++i ) 
      if ( graph_vertex_degree(g,i) > 3 ) {
         g->weights[i] = n;
         s = clique_find_single ( g, 3, 0, TRUE, opts);
         if ( s != NULL ) {
            if ( set_size(s) > LB ) {
               LB = set_size(s);
               set_copy(s,C);  /// C is the best maximal clique found
            } 
            Cs[n_c++]=set_duplicate(s);
         }
         g->weights[i] = 1;
      }

   /// Reduce the graph (if degree smaller than LB, then remove the vertex)
   for ( int i = 0; i < n; ++i ) {
      if ( graph_vertex_degree(g, i) > 0 && graph_vertex_degree(g, i) < LB ) { 
         for ( int j = 0; j < n && i != j; ++j )
            if ( GRAPH_IS_EDGE(g,i,j) )
               GRAPH_DEL_EDGE(g,i,j);
      }
   }

   /// Find a heuristic set of cliques used to post 
   /// the alldifferent constraints in the CP model
   //Cliques cliques = heuristic_cliques ( g, n_tries );    

   /// Call the CP model 
   fprintf(stdout, "Run CP model ...\n");
   int Xg = colorHeuristic ( g, &C, n_c, Cs, UB, timeout, memoryLimit, threads, saveMemory );
   fprintf(stdout, "\nX(G) = %d\n", Xg);

   /// Free the memory used by Cliquer
   if ( s != NULL )
      set_free(s);
   if ( C != NULL )
      set_free(C);
   free(Cs);
   free(opts);
   graph_free(g);

   return EXIT_SUCCESS;
}

/*
 *  Traveling Salesman Problem
 *
 *  Use the "nearest neighbor" or "greedy" algorithm as described at:
 *  http://en.wikipedia.org/wiki/Travelling_salesman_problem
 *
 *  Usage:
 *
 *      tsp [-v] [-s start_city] [-t seconds] input_of_cities [output_of_distance_and_path]
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <vector>

// ***********************************************************************
// * Declarations : (typically placed in a header file)
// ***********************************************************************

/* 
 * Cost - Symmetric matrix representing distance between any two vertices
 */ 
class Cost {
  public:
    void resize(int sz)                  { matrix.resize( sz, std::vector<int>( sz , 0 ) ); }
    int  getCost(int r, int c)           { return matrix[r][c]; }
    void setCost(int r, int c, int cost) { matrix[r][c] = cost; }

    // implemented as vector of vectors; accessed as matrix[i][j]
    std::vector< std::vector<int> > matrix;
} cost_matrix;

/*
 * City - (x,y) coords of a single city
 */
struct City {
    City(int xval, int yval) { x=xval; y=yval; }
    int x, y;
};

/*
 * Cities - Set/group of all cities represented by vertices in the graph
 */ 
class Cities {
  public:
    Cities() {}
    int  size() { return city_vec.size(); }
    void read(FILE *);
    void print(FILE *);
    void initCosts();

  private:
    std::vector<City> city_vec;
} cities;


/*
 * Path - represents a path through the graph 
 */
class Path {
  public:
    Path() : pathcost(0) {}
    int  calcPath(Cities c, int start_city);
    void printPath(FILE *);

  private:
    std::vector<int> path;
    int pathcost;
};

/*
 * Global data for thread control
 */
bool verbose_flag = false;
int  global_best_length = INT_MAX;
Path *global_best_path = NULL;
time_t global_stop_time = 0;
pthread_mutex_t m;


// ***********************************************************************
// * Support method implementations
// ***********************************************************************

void Cities::read(FILE *f)
{
    char buffer[1000];
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        int id, x, y;
        sscanf(buffer, "%d %d %d", &id, &x, &y);
        city_vec.push_back(City(x, y));
    }
}

void Cities::print(FILE *f)
{
    if (f) {
        for (int i=0; i<city_vec.size(); i++)
            fprintf(f, "%d %d %d\n", i, city_vec[i].x, city_vec[i].y);
    }
}



/*
 *  initCosts - populate the cost matrix
 */
void Cities::initCosts()
{
    int num_cities = city_vec.size();
    cost_matrix.resize(num_cities);
    for (int r=0; r<num_cities; r++) {
        for (int c=r; c<num_cities; c++) {
           int delx = (city_vec[r].x - city_vec[c].x);
           int dely = (city_vec[r].y - city_vec[c].y);

           // "manually" round the calculation by adding 1/2 and truncating
           cost_matrix.matrix[r][c] = sqrt( delx*delx + dely*dely ) + 0.5;

           // symmetrix matrix, so cut calculations in half
           cost_matrix.matrix[c][r] = cost_matrix.matrix[r][c];
        }
    }
}

void Path::printPath(FILE* outfile)
{
    if (outfile) {
        fprintf(outfile, "%d\n", pathcost);
        for (int i=0; i<path.size(); i++) {
            fprintf(outfile, "%d\n", path[i]);
        }
    }
}


// ***********************************************************************
// * Main computation
// ***********************************************************************

/*
 *  calcPath - Find a path through the graph
 *
 *  This uses the "nearest neighbor" algorithm, which simply means that it chooses the
 *  next node based on which one is closest (and which hasn't already been visited).
 */
int Path::calcPath(Cities c, int start_city)
{
    // Each bit records if a vertex is already included in the path
    // For thread safety reasons, don't store flag in the City graph
    std::vector<bool> inpath(c.size(), false);
    inpath[start_city] = true; 

    // Allow the calling routine to determine the start city (allows multiple/concurrent searches)
    int curr_city = start_city;
    path.push_back(start_city);
    for (int i=1; i<c.size(); i++) {

        // Scan the "curr_city" row to find nearest (unvisited) neighbor
        int closest = -1;
        int closest_cost = INT_MAX;
        for (int ix=0; ix<c.size(); ix++) {
            if ((!inpath[ix]) && (cost_matrix.getCost(curr_city, ix) < closest_cost)) {
                closest = ix; 
                closest_cost = cost_matrix.getCost(curr_city, closest);
            }
        }
        assert(closest != -1);
        inpath[closest] = true;

        // add this city to the path 
        path.push_back(closest);
        pathcost += cost_matrix.getCost(curr_city, closest);

        // prepare for next iteration
        curr_city = closest;
    }

    // Now add in the cost of the final link back to the initial city
    pathcost += cost_matrix.getCost(curr_city, start_city); 

    return pathcost;
}

typedef struct {
    int start, end;
} bounds_t;

void* runthread(void* v)
{
    bounds_t* b = (bounds_t*)v;
    int local_best_length = INT_MAX;
    Path *local_best_path = NULL;

    if (b->start > cities.size())
        fprintf(stderr, "Start city exceeds city count\n"), exit(-1);
    if (b->end > cities.size())
        b->end = cities.size();

    for (int start_city=b->start; start_city < b->end; start_city++) {

        // Honor the stop time if it has been specified
        if ((global_stop_time > 0) && (time(NULL) > global_stop_time)) {
            break;
        }

        // Perform the path calculation
        Path *path = new Path;
        int length = path->calcPath(cities, start_city);

        if (verbose_flag)
            printf("Length of path starting at %6d is %6d\n", start_city, length);
        if (length < local_best_length) {
            local_best_length = length;
            if (local_best_path)
                delete local_best_path;
            local_best_path = path;
        }
        else {
           delete path;
        }
    }

    pthread_mutex_lock(&m);
    if (local_best_length < global_best_length) {
       global_best_length = local_best_length;
       if (global_best_path)
           delete global_best_path;
       global_best_path = local_best_path;
    }
    pthread_mutex_unlock(&m);
    return 0;
}


/*
 *  Traveling Salesman Problem
 */
int
main(int argc, char* argv[])
{
    int  num_threads = 1;
    int  start_city = 0;
    FILE *infile = NULL, *outfile = NULL;

    verbose_flag = false;

    //  *******************************************************************
    //  Argument parsing 
    //  *******************************************************************
    if (argc == 1) {
        fprintf(stderr, "Usage: %s [-v] [-t seconds] inputfile [outfile]\n", argv[0]);
        fprintf(stderr, "-v     # Prints results of each full path computation\n"
                        "-s N   # Use city N to begin the path search\n"
                        "-T N   # Stops computing paths once N seconds exceeded\n"
                        "-t N   # Specifies the number of threads to execute\n");
        exit(0);
    }

    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) 
            verbose_flag = true;
        else if (strcmp(argv[i], "-s") == 0) {
            i++;
            start_city = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-T") == 0) {
            i++;
            int timelimit = atoi(argv[i]);
            global_stop_time = time(NULL) + timelimit;
        }
        else if (strcmp(argv[i], "-t") == 0) {
            i++;
            num_threads = atoi(argv[i]);
        }
        else if (infile == NULL) {
            if ( (infile = fopen(argv[i], "r")) == NULL)
                perror("failed to open input file"), exit(-1);
        }
        else if (outfile == NULL) { 
            if ( (outfile = fopen(argv[i], "w")) == NULL )
                perror("failed to open output file"), exit(-1);
        }
        else {
            fprintf(stderr, "Error on argument: '%s'\n", argv[i]);
            exit(-1);
        }
    }
    if (!infile) {
        fprintf(stderr, "%s : No input file specified ... exiting\n", argv[0]);
        exit(-1);
    }

    //  *******************************************************************
    //  Basic Algorithm -- call "calcPath"
    //  *******************************************************************
    cities.read(infile);
    cities.initCosts();
    // We will replace this single call to "calcPath()" with a threaded 
    // search, testing every city as a starting point.
    // int best_length = best_path->calcPath(cities, start_city);


    //  *******************************************************************
    //  Threaded Algorithm
    //    
    //  For M cities, divide them across N threads and calculate the path
    //  length using each city as a different starting point.
    //  *******************************************************************
    std::vector<bounds_t> bounds(num_threads);

    // We will run from start_city to cities.size()

    int range = (cities.size() - start_city) / num_threads;
    std::vector<pthread_t> thr_id(num_threads);
    for (int i=0; i<num_threads; i++) { 

        // Calculate the range of "start cities" for each thread to work on
        // If the cities don't divide evenly into the number of threads, then
        // have the last thread clean up the left over.
        bounds[i].start = i*range + start_city;
        bounds[i].end = bounds[i].start + range;
        if (i == num_threads -1)                // if last thread ...
            bounds[i].end = cities.size() + 1;  //  ... finish all the cities

        // Start each thread on their respective range of start cities
        if (pthread_create(&thr_id[i], NULL, runthread, &bounds[i]) < 0)
            perror("pthread_create"), exit(-1);
    }

    // Wait for all of the threads to complete
    for (int i=0; i<num_threads; i++) { 
        if (pthread_join(thr_id[i], NULL) < 0)
            perror("pthread_join"), exit(-1);
    }

    // printf("length = %d\n", best_length);
    global_best_path->printPath(outfile);
    delete global_best_path;

    return 0;
}

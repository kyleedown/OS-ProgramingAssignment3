#include "rw_log.h"

struct config {
    int capacity;
    int readers;
    int writers;
    int writer_batch;
    int seconds;
    int rd_us;
    int wr_us;
    int dump_csv;
};

static void print_usage(const char *progname){
    fprintf(stderr,
    "Usage: %s [options]\n"
    "Options:\n"
    "-c, --capacity <N> Log capacity (default 1024)\n"
    "-r, --readers <N> Number of reader threads (default 6)\n"
    "-w, --writers <N> Number of writer threads (default 4)\n"
    "-b, --writer-batch <N> Entries written per writer section (default 2)\n"
    "-s, --seconds <N> Total run time (default 10)\n"
    "-R, --rd-us <usec> Reader sleep between operations (default 2000)\n"
    "-W, --wr-us <usec> Writer sleep between operations (default 3000)\n"
    "-d, --dump Dump final log to log.csv\n"
    "-h, --help Show this help message\n",
    progname);
}

static void parse_args(int argc, char **argv, struct config *cfg){
    // Set defaults
    cfg->capacity = 1024;
    cfg->readers = 6;
    cfg->writers = 4;
    cfg->writer_batch = 2;
    cfg->seconds = 10;
    cfg->rd_us = 2000;
    cfg->wr_us = 3000;
    cfg->dump_csv = 0;

    int opt;
    extern char *optarg;

    struct option long_options[] = {
        {"capacity",required_argument,NULL,'c'},
        {"readers",required_argument,NULL,'r'},
        {"writers",required_argument,NULL,'w'},
        {"writer-batch",required_argument,NULL,'b'},
        {"seconds",required_argument, NULL, 's'},
        {"rd-us",required_argument, NULL, 'R'},
        {"wr-us",required_argument, NULL, 'W'}
    };

    while((opt = getopt_long_only(argc,argv,"",long_options,NULL))!= -1){
        switch (opt)
        {
        case 'c':
            if(atoi(optarg)){
                printf("Capacity is not valid and was set to the default value");
                break;
            }
            capacity = (size_t)optarg;
            break;
        case 'r':
            reader_count = atoi(optarg);
            printf("Creating Readers: %d", reader_count);
            break;
        case 'w':
            writer_count = atoi(optarg);
            printf("Creating Writers: %d", writer_count);
            break;
        case 'b':
            writer_batch = atoi(optarg);
            printf("Entries writers append: %d", writer_batch);
            break;
        case 's':
            runtime = atoi(optarg);
            printf("runtime: %d", runtime);
            break;
        case 'R':
            reader_sleep = atoi(optarg);
            printf("reader sleep: %d", reader_sleep);
            break;
        case 'W':
        writer_sleep = atoi(optarg);
            printf("Writer sleep: %d", writer_sleep);
            break;
        default:
            break;
        }
    } 


}


// void signalHandler(int sig){
//     stop_flag = true;
//     exit(sig);
// }

int rwlog_create(size_t capacity){
    printf("rwlog_create called");
    return 1;
}

int create_readers(int count, int delay){
    printf("reader_create called");
    return 1;
}

int create_writers(int count, int batch, int delay){
    printf("writer_create called");
    return 1;
}

int main (int argc, char *argv[]){


    //parse command line arguments
    // parse_args(argc,argv,);

    //Initialize the monitor
    rwlog_create(capacity);

    //Set up program stop mechanics
    stop_flag = false;
    /* 
    signal(SIGINT,signalHandler);
    */

    //Launch Worker threads
    create_readers(reader_count,reader_sleep);
    create_writers(writer_count,writer_batch,writer_sleep);
    //wait for runtime
    sleep(runtime);
    stop_flag = true;
    //join threads
    //Optional: dump the log
    //Compute and print metrics
    //cleanup
    }

    


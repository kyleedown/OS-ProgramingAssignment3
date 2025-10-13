#include "rw_log.h"

int main (int argc, char *argv[]){
    //parse command line arguments
    int opt;
    extern char *optarg;

    struct option long_options[] = {
        {"capacity",required_argument,NULL,'c'},
        {"readers",required_argument,NULL,'r'},
        {"writers",required_argument,NULL,'w'},
        {"writer-batch",required_argument,NULL,'b'},
        {"seconds",required_argument, NULL, 's'},
        {"rd-us",required_argument, NULL, 't'},
        {"wr-us",required_argument, NULL, 'p'}
    };

    while((opt = getopt_long_only(argc,argv,"",long_options,NULL))!= -1){
        switch (opt)
        {
        case 'c':
            capacity = atoi(optarg);
            printf("Capacity is ", capacity);
            break;
        case 'r':
            reader_count = atoi(optarg);
            printf("Creating Readers: %s", reader_count);
            break;
        case 'w':
            writer_count = atoi(optarg);
            printf("Creating Writers: %s", writer_count);
            break;
        case 'b':
            writer_batch = atoi(optarg);
            printf("Entries writers append: %s", writer_batch);
            break;
        case 's':
            runtime = atoi(optarg);
            printf("runtime: %s", runtime);
            break;
        case 't':
            reader_sleep = atoi(optarg);
            printf("reader sleep: %s", reader_sleep);
            break;
        case 'p':
        writer_sleep = atoi(optarg);
            printf("Writer sleep: %s", writer_sleep);
            break;
        default:
            break;
        }
    } 


    //Initialize the monitor
    //Set up program stop mechanics
    //Launch Worker threads
    //wait for runtime
    //join threads
    //Optional: dump the log
    //Compute and print metrics
    //cleanup
    }

    


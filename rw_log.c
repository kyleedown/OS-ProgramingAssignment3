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
            printf("Capacity is %s", optarg);
            break;
        case 'r':
            printf("Creating Readers: %s", optarg);
            break;
        case 'w':
            printf("Creating Writers: %s", optarg);
            break;
        case 'b':
            printf("Entries writers append: %s", optarg);
            break;
        case 's':
            printf("runtime: %s", optarg);
            break;
        case 't':
            printf("reader sleep: %s", optarg);
            break;
        case 'p':
            printf("Writer sleep: %s", optarg);
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

    


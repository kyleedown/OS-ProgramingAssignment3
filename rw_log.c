#include "rw_log.h"

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
        case 't':
            reader_sleep = atoi(optarg);
            printf("reader sleep: %d", reader_sleep);
            break;
        case 'p':
        writer_sleep = atoi(optarg);
            printf("Writer sleep: %d", writer_sleep);
            break;
        default:
            break;
        }
    } 

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

    


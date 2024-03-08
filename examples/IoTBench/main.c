/* File: main.c
        This file contains the framework to acquire memory run the benchmark and report the result.
*/

#include "benchmark.h"
uint8_t list_memblk[TOTAL_DATA_SIZE] = {0};
uint8_t mat_memblk[TOTAL_DATA_SIZE] = {0};
uint8_t conv_memblk[TOTAL_DATA_SIZE] = {0};
// result
uint16_t result[RESULT_SIZE] = {0};

volatile sint16_t list_seed = 0x66;
volatile sint32_t mat_seed = 0x11;
volatile sint32_t conv_seed = 0x7;

/*Function: start_time & stop_time
    record the time
*/
/*
#define TIMESTRUCT struct timespec
TIMESTRUCT time_start;
TIMESTRUCT time_end;

void
start_time(){
    time_start.tv_sec = 0;
    time_start.tv_nsec = 0;
    time_end.tv_sec = 0;
    time_end.tv_nsec = 0;
    clock_gettime(CLOCK_REALTIME, &time_start);
    return;
}

void 
stop_time(){
    clock_gettime(CLOCK_REALTIME, &time_end);
    return;
}
*/

//#define CLOCKS_PER_SEC  ((__clock_t) 1000000)
#define TIMESTRUCT clock_t
TIMESTRUCT time_start;
TIMESTRUCT time_end;
float time_run;
//double time_run;

uint32_t num_iterate = NUM_ITERATE;

void
start_time(){
    time_start = 0;
    time_end = 0;
    time_run = 0.0;
    time_start = clock();
    return;
}

void 
stop_time(){
    time_end = clock();
    return;
}

void
run_time(){
    //stop_time();
    time_run = (time_end - time_start) * 1.0 /CLOCKS_PER_SEC;
}
/* Function: report
    report the result.

*/

void 
report(){
    printf("--------------------------------------------------------------\n");
    printf("DataSize/Bytes:\n");
    printf("%ld\n", TOTAL_DATA_SIZE * sizeof(uint8_t) * 3);
    printf("DataDim:\n");
    printf("%d\n", DATA_DIM);
    printf("DataType:\n");
    //printf("%d\n", TEST_TYPE);
    #if TEST_TYPE == INT_TYPE
    printf("INT_TYPE\n");
    #elif TEST_TYPE == FP32_TYPE
    printf("FP32_TYPE\n");
    #elif TEST_TYPE == FP64_TYPE
    printf("FP64_TYPE\n");
    #endif
    //printf("tick_begin:\n");
    //printf("%ld\n", time_start);
    //printf("tick_end:\n");
    //printf("%ld\n", time_end);
    printf("TotalTicks/Ticks:\n");
    printf("%ld\n", time_end - time_start);
    printf("TotalTime/Secs:\n");
    printf("%f\n", time_run);
    printf("Iterations:\n");
    printf("%d\n",num_iterate);
    printf("Iterations/Secs:\n");
    printf("%f\n",num_iterate/time_run);
    printf("--------------------------------------------------------------\n");
    return;
}
/*
void 
report(){
    printf("------------------------------BEGIN---------------------------\n");
    printf("time_start:\n");
    printf("%ld s %ld ns\n", time_start.tv_sec, time_start.tv_nsec);
    printf("time_end:\n");
    printf("%ld s %ld ns\n", time_end.tv_sec, time_end.tv_nsec);
    printf("run time:\n");
    printf("%ld s %ld ns\n", (time_end.tv_sec - time_start.tv_sec), (time_end.tv_nsec - time_start.tv_nsec));
    printf("------------------------------END----------------------------\n");
    return;
}
*/

/* Function: main
    This function consists of the following steps:
        1 - initialize memory block.  -- benchmark.h
        2 - run benchmark and record the time.
        3 - report the result.

*/

int 
main(){
    /*base on subspace*/
    /*initialize*/
    list_node *memblk = list_init(TOTAL_DATA_SIZE, (list_node *)list_memblk, list_seed);
    
    mat_params mat_p[1] = {0};
    uint32_t mat_size = matrix_init(TOTAL_DATA_SIZE, (DATA_TYPE *)mat_memblk, mat_seed, mat_p);
    
    conv_params conv_p[1] = {0};
    conv_init(TOTAL_DATA_SIZE, (DATA_TYPE *)conv_memblk, conv_seed, conv_p);

    /*TEST*/
    list_data info;
    #if TEST_TYPE==INT_TYPE
        #if DATA_DIM == 1
        for(int i = 0; i < DIM_X; i++){
            info.data[i] = (list_seed) ^ (i);
        }
        #elif DATA_DIM == 2
        for(int i = 0; i < DIM_X; i++){
            for(int j = 0; j < DIM_Y; j++){
                info.data[i][j] = (list_seed) ^ (i + j);
            }
        }
        #elif DATA_DIM == 3
        for(int i = 0; i < DIM_X; i++){
            for(int j = 0; j < DIM_Y; j++){
                for(int k = 0; k < DIM_Z; k++){
                    info.data[i][j][k] == (list_seed) ^ (i + j + k);
                }
            }
        }
        #endif
    #else
        #if DATA_DIM == 1
        for(int i = 0; i < DIM_X; i++){
            info.data[i] = (((list_seed << 16) | list_seed) + i)/((list_seed << 10) * 1.3);
        }
        #elif DATA_DIM == 2
        for(int i = 0; i < DIM_X; i++){
            for(int j = 0; j < DIM_Y; j++){
                info.data[i][j] = (((list_seed << 16) | list_seed) + (i^j))/((list_seed << 10) * 1.3);
            }
        }
        #elif DATA_DIM == 3
        for(int i = 0; i < DIM_X; i++){
            for(int j = 0; j < DIM_Y; j++){
                for(int k = 0; k < DIM_Z; k++){
                    info.data[i][j][k] == (((list_seed << 16) | list_seed) + (i^j^k))/((list_seed << 10) * 1.3);
                }
            }
        }
        #endif
    #endif

    info.idx = 5;

    #if TEST_TYPE == INT_TYPE
    uint32_t val = 2;
    #else
    float val = 2.1;
    #endif

    /*iterate*/
    start_time();
    for(int i = 0; i < /*num_iterate*/ NUM_ITERATE; i++){
        /*list_search_sort*/
        //printf("Iteration : %d\n", i);
        
        
        list_search_all(memblk, &info);

        if(SORT_METHOD == MERGESORT){
            memblk = list_mergesort(memblk, DATA_CMP);
            memblk = list_mergesort(memblk, IDX_CMP);
        }
        
        //list_search(memblk. &info, IDX_FIND);

        /*matrix*/
        
        matrix_add_const(mat_p->N, mat_p->A, val);
        matrix_mul_const(mat_p->N, mat_p->A, val, mat_p->C);
        matrix_mul_matrix(mat_p->N, mat_p->A, mat_p->B, mat_p->C);
        
        /*conv*/
        
        #if CONV_DIM==2
        conv2D(conv_p->in, conv_p->out, conv_p->InChannel, conv_p->InWidth, conv_p->InHeight, conv_p->filter, conv_p->filter_size, conv_p->stride, conv_p->bias, conv_p->OutChannel);
        #elif CONV_DIM==1
        conv1D(conv_p->in, conv_p->out, conv_p->InChannel, conv_p->InWidth, conv_p->filter, conv_p->filter_size, conv_p->stride, conv_p->bias, conv_p->OutChannel);
        #elif CONV_DIM==3
        conv3D(conv_p->in, conv_p->out, conv_p->InChannel, conv_p->InWidth, conv_p->InHeight, conv_p->InDepth, conv_p->filter, conv_p->filter_size, conv_p->stride, conv_p->bias, conv_p->OutChannel);
        #endif
        

        /*
        run_time();
        
        if(i == (num_iterate - 1) && time_run < MIN_SECS){
            num_iterate = num_iterate * 5;
        }
        */
    }
    stop_time();
    run_time();


    /*report result*/
    report();
    return 0;
}
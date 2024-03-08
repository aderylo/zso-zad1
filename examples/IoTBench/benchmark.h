#ifndef BENCHMARK_H
#define BENCHMARK_H
#include<time.h>
#include "type.h"
/*basic*/

/*evaluation subspace*/
/*Data size*/
/*define total size for data algorithms will operate on, EXCEPT result[] */
#ifndef TOTAL_DATA_SIZE
#define TOTAL_DATA_SIZE 2 * 1024
#endif

/*Data precision*/
/*define the data type to be operated*/
#define INT_TYPE  0
#define FP32_TYPE 1
#define FP64_TYPE 2
/*INT8 FP64*/

#ifndef TEST_TYPE 
#define TEST_TYPE INT_TYPE
#endif

#define NORMAL 0
#define LOW_ACCURACY 1
#define HIGH_ACCURACY 2

#ifndef CALCULATE_ACCURACY
#define CALCULATE_ACCURACY NORMAL
#endif

#define DIM_X 2
#define DIM_Y 2
#define DIM_Z 2

/*data dimension*/
#ifndef DATA_DIM 
#define DATA_DIM 1
#endif

#ifndef CONV_DIM
#define CONV_DIM DATA_DIM
#endif

/*result space, used for report*/
#ifndef RESULT_SIZE
#define RESULT_SIZE TOTAL_DATA_SIZE/3
#endif

extern volatile sint16_t list_seed;
extern volatile sint32_t mat_seed;
extern volatile sint32_t conv_seed;

/*LIST_SORT*/
#define MERGESORT 1
#define QUICKSORT 2
#ifndef SORT_METHOD
#define SORT_METHOD MERGESORT
#endif


#if TEST_TYPE==INT_TYPE
#define DATA_TYPE uint32_t
#elif TEST_TYPE==FP64_TYPE
#define DATA_TYPE double
#else
#define DATA_TYPE float
#endif

#if TEST_TYPE==INT_TYPE
#define MAT_DATA uint32_t
#elif TEST_TYPE==FP64_TYPE
#define MAT_DATA double
#else
#define MAT_DATA float
#endif

/*compare method used for mergesort*/
#define IDX_CMP 0
#define DATA_CMP 1

#ifndef CMP_METHOD
#define CMP_METHOD DATA_CMP
#endif

/*search for item by idx or data value*/
#define DATA_FIND 0
#define IDX_FIND 1

/*interate times*/
#ifndef NUM_ITERATE
#define NUM_ITERATE 10
#endif

/*interate secs*/
#ifndef MIN_SECS
#define MIN_SECS 5
#endif

/*list*/
#if DATA_DIM==1
typedef struct list_data_s{
    DATA_TYPE data[DIM_X]; 
    uint16_t idx;
}list_data;
#elif DATA_DIM==2
typedef struct list_data_s{
    DATA_TYPE data[DIM_X][DIM_Y]; 
    uint16_t idx;
}list_data;
#elif DATA_DIM==3
typedef struct list_data_s{
    DATA_TYPE data[DIM_X][DIM_Y][DIM_Z]; 
    uint16_t idx;
}list_data;
#endif 

typedef struct list_node_s{
    struct list_node_s *next;
    struct list_data_s *info;
}list_node;

/*matrix*/
typedef struct mat_params_s{
    uint32_t N;
    MAT_DATA *A;
    MAT_DATA *B;
    MAT_DATA *C;
}mat_params;


/*conv*/
typedef struct conv_params_s{
    DATA_TYPE *in;
    DATA_TYPE *out;
    DATA_TYPE *filter;
    DATA_TYPE *bias;
    uint32_t stride;
    uint32_t InChannel;
    uint32_t InWidth;
    #if CONV_DIM==2 || CONV_DIM==3
    uint32_t InHeight;
    #endif
    uint32_t OutChannel;
    uint32_t filter_size;
    #if CONV_DIM==3
    uint32_t InDepth;
    #endif
}conv_params;

extern uint8_t list_memblk[TOTAL_DATA_SIZE];
extern uint16_t result[RESULT_SIZE];
extern uint8_t mat_memblk[TOTAL_DATA_SIZE];
extern uint8_t conv_memblk[TOTAL_DATA_SIZE];

#ifndef INCHANNEL
#define INCHANNEL 3 // if INCHANNEL==1, the input data is two-dimentional
#endif

#ifndef OUTCHANNEL
#define OUTCHANNEL 1
#endif

#ifndef STRIDE
#define STRIDE 1
#endif

#ifndef FILTER_SIZE
#define FILTER_SIZE 2 // NOTE: the InHeight(Width) should be larger than filter size
#endif


/*list benchmark functions*/
list_node *list_init(uint32_t blksize, list_node *memblk, sint16_t seed);// list_search_sort.c
list_node *list_insert(list_node *insert_point, list_data *info, list_node **memblk, list_data **datablk, list_node *nodes_end, list_data *data_end);// list_search_sort.c

list_node *list_mergesort(list_node *list, uint8_t cmp);// list_search_sort.c
//list_node *list_quicksort(list_node *list);

list_node *list_search(list_node *list, list_data *info, uint8_t key);// list_search_sort.c
uint16_t *list_search_all(list_node *list, list_data *info);// list_search_sort.c
//list_node *list_tree_search(list_node *list, list_data *info);
//list_node *list_binary_search(list_node *list, list_data *info);

//list_node *list_statistic(list_node *list, list_data *info);

/*matrix benchmark functions*/
uint32_t matrix_init(uint32_t blksize, DATA_TYPE *memblk, sint32_t seed, mat_params *p);//matrix.c
void matrix_add_const(uint32_t N, MAT_DATA *A, MAT_DATA val);//matrix.c
void matrix_mul_const(uint32_t N, MAT_DATA *A, MAT_DATA val, MAT_DATA *C);//matrix.c
void matrix_mul_matrix(uint32_t N, MAT_DATA *A, MAT_DATA *B, MAT_DATA *C);//matrix.c
//void app_matrix_mul_matrix(uint32_t N, MAT_DATA *A, MAT_DATA *B, MAT_DATA *C);

/*conv benchmark functions*/
void conv_init(uint32_t blksize, DATA_TYPE *memblk, sint32_t seed, conv_params *p);
/*
void conv2D(unsigned char *in, unsigned char *out, int Ic, int Iw, int Ih, unsigned char *filter, int K, int S, unsigned char *bias, int Oc);
void conv1D(unsigned char *in, unsigned char *out, int Ic, int Iw, int Ih, unsigned char *filter, int K, int S, unsigned char *bias, int Oc);
void conv3D(unsigned char *in, unsigned char *out, int Ic, int Iw, int Ih, int Id, unsigned char *filter, int K, int S, unsigned char *bias, int Oc);
*/
void conv2D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, int Ih, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc);
//void conv1D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, int Ih, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc);
void conv1D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc);
void conv3D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, int Ih, int Id, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc);
#endif
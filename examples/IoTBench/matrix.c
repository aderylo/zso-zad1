#include "benchmark.h"
/*Function: matrix_init
    Initialize memory block for matrix benchmarking.

    Parameters: 
        blksize - Size of memory to be initialized(/bytes).
        memblk - Pointer to memory block.
        seed - Actual values are assigned according to seed.
        p - pointer to <mat_params>
    
    Returns:
        Matrix size
    
    Description:
        the matrices are of the same size -- N*N, and N is determined by the size of the memory block.
*/
uint32_t matrix_init(uint32_t blksize, DATA_TYPE *memblk, sint32_t seed, mat_params *p){
    //printf("matrix_init...#-#\n");
    MAT_DATA *A;
    MAT_DATA *B;
    MAT_DATA *C;
    uint32_t N = 0;
    uint32_t i = 0, j = 0, k = 0;
    uint32_t tmp = 0;
    float div = seed * 1.3;
    #if DATA_DIM==1
    N = blksize/(sizeof(MAT_DATA) * 3);
    A = (MAT_DATA *)(memblk);
    B = (MAT_DATA *)(A + N);
    C = (MAT_DATA *)(B + N);
        #if TEST_TYPE==INT_TYPE
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        A[i] = (tmp ) | i;
        B[i] = (tmp ) | tmp;
    }
        #else
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        A[i] = ((tmp << 2) | tmp) / div;
        B[i] = ((tmp << 2) | (tmp - 1)) / div;
    }
        #endif
    

    #elif DATA_DIM==2
    i = 0, j = 0;
    uint32_t tmp2;
    while(j < blksize){
        i++;
        j=i*i*sizeof(MAT_DATA)*3;
    }
    N = i - 1;
    A = (MAT_DATA *)(memblk);
    B = (MAT_DATA *)(A + N * N);
    C = (MAT_DATA *)(B + N * N);
        #if TEST_TYPE==INT_TYPE
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        for(j = 0; j < N; j++){
            tmp2 = (seed ^ j);
            A[i * N + j] = (tmp2 ) | tmp2;
            B[i * N + j] = (tmp2 ) | tmp;
        }
    } 
        #else
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        for(j = 0; j < N; j++){     
            tmp2 = (seed ^ j);
            A[i * N + j] = ((tmp2 << 2) | tmp2) / div;
            B[i * N + j] = ((tmp2 << 2) | tmp) / div;
        }
    } 
        #endif
    #elif DATA_DIM==3
    i = 0, j = 0, k = 0;
    uint32_t tmp2, tmp3;
    while(j < blksize){
        i++;
        j = i * i * i * sizeof(MAT_DATA) * 3;
    }
    N = i - 1;
    A = (MAT_DATA *)(memblk);
    B = (MAT_DATA *)(A + N * N * N);
    C = (MAT_DATA *)(B + N * N * N);
        #if TEST_TYPE==INT_TYPE
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        for(j = 0; j < N; j++){
            tmp2 = (seed ^ j);
            for(k = 0; k < N; k++){
                tmp3 = (seed ^ k);
                A[i * N * N + j * N + k] = (tmp3 ) | tmp;
                B[i * N * N + j * N + k] = (tmp3 ) | tmp2;
            }
        }
    } 
        #else
    
    for(i = 0; i < N; i++){
        tmp = (seed ^ i);
        for(j = 0; j < N; j++){
            tmp2 = (seed ^ j);
            for(k = 0; k < N; k++){
                tmp3 = (seed ^ k);
                A[i * N * N + j * N + k] = ((tmp3 << 2) | tmp) / div;
                B[i * N * N + j * N + k] = ((tmp3 << 2) | tmp2) / div;
            }
        }
    }     

        #endif
    #endif 

    p->N = N;
    p->A = A;
    p->B = B;
    p->C = C;

    return N;
}

/*Function: matrix_add_const
    Add a constant to all elements of orignal matrix A.

    Parameters:
        N - Matrix Dimension
        A - Original matrix 
        val - constant

    Note:
        result is stored in A.
*/
void matrix_add_const(uint32_t N, DATA_TYPE *A, DATA_TYPE val){
    //printf("matrix_add_const...(*-*)\n");
    #if CALCULATE_ACCURACY==NORMAL

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        A[i] += val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            A[i * N + j] += val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                A[i * N * N + j * N + k] += val;
            }
        }
    }
    #endif

    #elif CALCULATE_ACCURACY==LOW_ACCURACY

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        A[i] = (float)A[i]+(float)val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            A[i * N + j] = (float)A[i * N + j]+(float)val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                A[i * N * N + j * N + k] = (float)A[i * N * N + j * N + k]+(float)val;
            }
        }
    }

    #endif

    #elif CALCULATE_ACCURACY==HIGH_ACCURACY

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        A[i] = (double)A[i]+(double)val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            A[i * N + j] = (double)A[i * N + j]+(double)val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                A[i * N * N + j * N + k] = (double)A[i * N * N + j * N + k]+(double)val;
            }
        }
    }
    
    #endif

    #endif
}

/*Function: matrix_mul_const
    Multiply a constant to all elements of orignal matrix A

    Parameters:
        N - Matrix Dimension
        A - original matrix 
        val - constant
        C - result        
*/
void matrix_mul_const(uint32_t N, DATA_TYPE *A, DATA_TYPE val, DATA_TYPE *C){
    //printf("matrix_mul_const...(*-*)\n");
    /*for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            C[i * N + j] = A[i * N + j] * val;
        }
    }
    */
    #if CALCULATE_ACCURACY==NORMAL

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        C[i] = A[i] * val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
           C[i * N + j] = A[i * N + j] * val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i * N * N + j * N + k] = A[i * N * N + j * N + k] * val;
            }
        }
    }
    #endif

    #elif CALCULATE_ACCURACY==LOW_ACCURACY

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        C[i] = (float)A[i] * (float)val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
           C[i * N + j] = (float)A[i * N + j] * (float)val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i * N * N + j * N + k] = (float)A[i * N * N + j * N + k] * (float)val;
            }
        }
    }
    #endif

    #elif CALCULATE_ACCURACY==HIGH_ACCURACY

    #if DATA_DIM==1
    for(int i = 0; i < N; i++){
        C[i] = (double)A[i] * (double)val;
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
           C[i * N + j] = (double)A[i * N + j] * (double)val;
        }
    }
    #elif DATA_DIM==3
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i * N * N + j * N + k] = (double)A[i * N * N + j * N + k] * (double)val;
            }
        }
    }
    #endif

    #endif
}

/*Function: matrix_mul_matrix
    Multiply a matrix A by a matrix B

    Parameters:
        N - Matrix Dimension
        A - operand
        B - operand
        C - result
        
*/
void matrix_mul_matrix(uint32_t N, DATA_TYPE *A, DATA_TYPE *B, DATA_TYPE *C){
    //printf("matrix_mul_matrix...(*-*)\n");
    #if CALCULATE_ACCURACY==NORMAL

    #if DATA_DIM==1
    C[0] = 0;
    for(int i = 0; i < N; i++){
        C[0] += A[i] * B[i];
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            C[i * N + j] = 0;
            for(int k = 0; k < N; k++){
                C[i * N + j] += A[i * N + k] * B[k * N + j];
            }
        }
    }
    #elif DATA_DIM==3
    // TODO:
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i*N*N + j*N + k] = 0;
                for(int l = 0; l < N; l++){
                    C[i*N*N + j*N +k] += A[i*N*N + j*N + l] * B[i*N*N + l*N + j];
                }
            }
        }
    }
    #endif

    #elif CALCULATE_ACCURACY==LOW_ACCURACY

    #if DATA_DIM==1
    C[0] = 0;
    for(int i = 0; i < N; i++){
        C[0] = (float)C[0] + (float)A[i] * (float)B[i];
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            C[i * N + j] = 0;
            for(int k = 0; k < N; k++){
                C[i * N + j] =(float)C[i * N + j] + (float)A[i * N + k] * (float)B[k * N + j];
            }
        }
    }
    #elif DATA_DIM==3
    // TODO:
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i*N*N + j*N + k] = 0;
                for(int l = 0; l < N; l++){
                    C[i*N*N + j*N +k] = (float)C[i*N*N + j*N +k] + (float)A[i*N*N + j*N + l] * (float)B[i*N*N + l*N + j];
                }
            }
        }
    }
    #endif

    #elif CALCULATE_ACCURACY==HIGH_ACCURACY

    #if DATA_DIM==1
    C[0] = 0;
    for(int i = 0; i < N; i++){
        C[0] = (double)C[0] + (double)A[i] * (double)B[i];
    }
    #elif DATA_DIM==2
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            C[i * N + j] = 0;
            for(int k = 0; k < N; k++){
                C[i * N + j] =(double)C[i * N + j] + (double)A[i * N + k] * (double)B[k * N + j];
            }
        }
    }
    #elif DATA_DIM==3
    // TODO:
    for(int i = 0; i < N; i++){
        for(int j = 0; j < N; j++){
            for(int k = 0; k < N; k++){
                C[i*N*N + j*N + k] = 0;
                for(int l = 0; l < N; l++){
                    C[i*N*N + j*N +k] = (double)C[i*N*N + j*N +k] + (double)A[i*N*N + j*N + l] * (double)B[i*N*N + l*N + j];
                }
            }
        }
    }
    #endif

    #endif
}

/*Function: app_matrix_mul_matrix
    Approximately multiply a matrix A by a matrix B

    Parameters:
        N - Matrix Dimension
        A - operand
        B - operand
        C - result
        
*/
/*
void app_matrix_mul_matrix(uint32_t N, DATA_TYPE *A, DATA_TYPE *B, DATA_TYPE *C){

}
*/
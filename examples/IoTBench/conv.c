#include "benchmark.h"
/*Function:conv_init
    Initialize memory block for conv algorithm

    Parameters:
        blksize - Size of memory to be initialized(/bytes).
        memblk - Pointer to memory block.
        seed - Actual values are assigned according to seed.
        p - pointer to <conv_params>

    Description:
        Initialize input, filter, output, bias

    Note:
        CONV_DIM represents the freedom of filter's movement (1/2/3)
        INCHANNEL shows the data is two/three dimentional
        conv3D: filter_dimx == filter_dimy == fiter_dimz; InHeight == InWidth == InDepth
        conv2D: filter_dimx == filter_dimy; InHeigh == Inwidth
*/
void conv_init(uint32_t blksize, DATA_TYPE *memblk, sint32_t seed, conv_params *p){
    //printf("conv_init...:P\n");
    p->InChannel = INCHANNEL;
    p->OutChannel = OUTCHANNEL;
    p->stride = STRIDE;
    p->filter_size = FILTER_SIZE;
    uint32_t i = 0, j = 0, k = 0; // i - InWidth  & InHeight
    float div = seed * 1.3;

    /*calculate scale according to the memsize*/
    #if CONV_DIM==1
    i = 0, j = 0, k = 0;
    while(j < blksize){
        i++;
        //    input                    filters                                                                          output                   bias
        j = (i * p->InChannel + p->filter_size * p->InChannel * p->OutChannel + ((i - p->filter_size)/p->stride + 1) * 1 * p->OutChannel + p->OutChannel) * sizeof(DATA_TYPE);
    }
    #elif CONV_DIM==2  
    i = 0, j = 0, k = 0;  
    while(j < blksize){
        i++;
        //    input                    filters                                                                          output                                                                       bias
        j = (i * i * p->InChannel + p->filter_size * p->filter_size  * p->InChannel * p->OutChannel + ((i - p->filter_size)/p->stride + 1) * ((i - p->filter_size)/p->stride + 1) * p->OutChannel + p->OutChannel) * sizeof(DATA_TYPE);
    }
    #elif CONV_DIM==3
    i = 0, j = 0, k = 0;
    while(j < blksize){
        i++;
        //    input                                                   filters                                                                                      output                                                                                          bias
        j = (i * i * i * p->InChannel + p->filter_size * p->filter_size * p->filter_size * p->InChannel * p->OutChannel + ((i - p->filter_size)/p->stride + 1) * ((i - p->filter_size)/p->stride + 1) * ((i - p->filter_size)/p->stride + 1) * p->OutChannel + p->OutChannel) * sizeof(DATA_TYPE);
    }
    #endif

    #if CONV_DIM==3
    p->InDepth = i - 1;
    p->InHeight = i - 1;
    p->InWidth = i - 1;
    p->in = (DATA_TYPE *)(memblk);
    p->out = (DATA_TYPE *)(memblk + p->InChannel * p->InHeight * p->InWidth * p->InDepth);
    int OutHeight = (p->InHeight - p->filter_size)/p->stride + 1;
    int OutWidth = (p->InWidth - p->filter_size)/p->stride + 1;
    int OutDepth = (p->InDepth - p->filter_size)/p->stride + 1;
    p->filter = (DATA_TYPE *)(p->out + p->OutChannel * OutHeight * OutWidth * OutDepth);
    p->bias = (DATA_TYPE *)(p->filter + p->filter_size * p->filter_size * p->filter_size * p->InChannel * p->OutChannel);
    #elif CONV_DIM==2
    p->InHeight = i - 1;
    p->InWidth = i - 1;
    p->in = (DATA_TYPE *)(memblk);
    p->out = (DATA_TYPE *)(memblk + p->InChannel * p->InHeight * p->InWidth);
    int OutHeight = (p->InHeight - p->filter_size)/p->stride + 1;
    int OutWidth = (p->InWidth- p->filter_size)/p->stride + 1;
    p->filter = (DATA_TYPE *)(p->out + p->OutChannel * OutHeight * OutWidth);
    p->bias = (DATA_TYPE *)(p->filter + p->filter_size * p->filter_size * p->InChannel * p->OutChannel);
    #elif CONV_DIM==1
    p->InWidth = i - 1;
    p->in = (DATA_TYPE *)(memblk);
    p->out = (DATA_TYPE *)(memblk + p->InChannel * p->InWidth);
    int OutWidth = (p->InWidth - p->filter_size)/p->stride + 1;
    p->filter = (DATA_TYPE *)(p->out + p->OutChannel * 1 * OutWidth);
    p->bias = (DATA_TYPE *)(p->filter + p->filter_size * p->InChannel * p->OutChannel);
    #endif

    /*input*/
    #if CONV_DIM==3
    for(i = 0; i < p->InChannel * p->InHeight * p->InWidth * p->InDepth; i++){
        #if TEST_TYPE == INT_TYPE
        p->in[i] = (seed ^ i);
        #else
        p->in[i] = ((seed << 2) | (seed ^ i))/div;
        #endif
    }
    #elif CONV_DIM==2
    for(i = 0; i < p->InChannel; i++){
        for(j = 0; j < p->InHeight; j++){
            for(k = 0; k < p->InWidth; k++){
                #if TEST_TYPE==INT_TYPE
                p->in[i * p->InWidth * p->InHeight + j * p->InWidth + k] = ((seed ^ i) ^ j) ^ k;
                #else
                p->in[i * p->InWidth * p->InHeight + j * p->InWidth + k] = ((seed << 2) | (((seed ^ i) ^ j) ^ k)) / div;
                #endif
            }
        }
    }
    #elif CONV_DIM==1
    for(i = 0; i < p->InChannel; i++){
        for(k = 0; k < p->InWidth; k++){
            #if TEST_TYPE==INT_TYPE
            p->in[i * p->InWidth + k] = ((seed ^ i)) ^ k;
            #else
            p->in[i * p->InWidth + k] = ((seed << 2) | (((seed ^ i)) ^ k)) / div;
            #endif
        }
    }
    #endif

    /*filter*/
    #if CONV_DIM==1
    for(i = 0; i < p->filter_size * p->InChannel * p->OutChannel; i++){
        #if TEST_TYPE == INT_TYPE
        p->filter[i] = (seed ^ i);
        #else
        p->filter[i] = ((seed << 2) | (seed ^ i))/div;
        #endif
    }
    #elif CONV_DIM==2
    for(i = 0; i < p->filter_size * p->filter_size  * p->InChannel * p->OutChannel; i++){
        #if TEST_TYPE == INT_TYPE
        p->filter[i] = (seed ^ i);
        #else
        p->filter[i] = ((seed << 2) | (seed ^ i))/div;
        #endif
    }
    #elif CONV_DIM==3
    for(i = 0; i < p->filter_size * p->filter_size * p->filter_size * p->InChannel * p->OutChannel; i++){
        #if TEST_TYPE == INT_TYPE
        p->filter[i] = (seed ^ i);
        #else
        p->filter[i] = ((seed << 2) | (seed ^ i))/div;
        #endif
    }
    #endif

    for(i = 0; i < p->OutChannel; i++){
        #if TEST_TYPE == INT_TYPE
        p->bias[i] = (seed ^i);
        #else
        p->bias[i] = (seed ^ i)/div;
        #endif
    }
    
    
}


/*Function: conv2D
    Two-dimensional convolution

    Parameters:
        in - input 
        out - output
        Ic - input channel
        Iw - input width
        Ih - input height
        filter - filter(K * K * Ic * Oc)
        K - size of filter
        S - stride
        bias - bias
        Oc - Output channel

    Note: NO padding.
*/
void conv2D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, int Ih, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc){
    //printf("conv2D...:D\n");
    int no, ni, x, y, kx, ky, iw, ih, OIdx, IIdx, WIdx, Oh, Ow;
    Ow = (Iw - K)/S + 1;
    Oh = (Ih - K)/S + 1;
    for(no = 0; no < Oc; no++){ // output channel(the number of filters)
        for(ni = 0; ni < Ic; ni++){ // input channel
            for(y = 0; y < Oh; y++){ // output height
                for(x = 0; x < Ow; x++){ // output width 
                    OIdx = no * Ow * Oh + y * Ow + x;   
                    if(ni == 0){
                        // output(no, x, y) = bias(no)
                        out[OIdx] = bias[no];
                    }                 
                    for(ky = 0; ky < K; ky++){ // weight h
                        for(kx= 0; kx < K; kx++){// weight w
                            iw = kx + x*S;
                            ih = ky + y*S;
                            // output(no, x, y) = input(ni, iw, ih) * weight(ni, no, kx, ky)
                            IIdx = ni * Iw * Ih + ih * Iw + iw;
                            WIdx = no * K * K * Ic + ni * K * K + ky * K + kx;
                            #if CALCULATE_ACCURACY==NORMAL
                            out[OIdx] += in[IIdx] * filter[WIdx];
                            #elif CALCULATE_ACCURACY==LOW_ACCURACY
                            out[OIdx] = (float)out[OIdx] + (float)in[IIdx] * (float)filter[WIdx];
                            #elif CALCULATE_ACCURACY==HIGH_ACCURACY
                            out[OIdx] = (double)out[OIdx] + (double)in[IIdx] * (double)filter[WIdx];
                            #endif
                        }
                    }
                }
            }
        }
    }
}

/*Function: conv1D
    One-dimensional convolution

    Parameters:
        in - input 
        out - output
        Ic - input channel
        Iw - input width
        filter - filter(K * Ic * Oc)
        K - size of filter
        S - stride
        bias - bias
        Oc - Output channel

    Note: NO padding.
    Description: the height of filter == the height of input == 1.
*/

void conv1D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc){
    //printf("conv1D...:D\n");
    int no, ni, x, y, kx, iw, ih, OIdx, IIdx, WIdx, Ow;
    Ow = (Iw - K)/S + 1;
    for(no = 0; no < Oc; no++){ // output channel(the number of filters)
        for(ni = 0; ni < Ic; ni++){ // input channel
            for(x = 0; x < Ow; x++){ // output width // for conv1D, x==0
                OIdx = no * Ow + x; 
                if(ni == 0){
                    // output(no, x, y) = bias(no)
                    out[OIdx] = bias[no];//initialize
                }                 
                for(kx = 0; kx < K; kx++){ // weight w
                    
                    iw = kx + x*S; //iw = kx;
                    // output(no, x) = input(ni, iw) * weight(ni, no, kx)
                    IIdx = ni * Iw + iw;
                    WIdx = no * K * Ic + ni * K + kx;
                    #if CALCULATE_ACCURACY==NORMAL
                    out[OIdx] += in[IIdx] * filter[WIdx];
                    #elif CALCULATE_ACCURACY==LOW_ACCURACY
                    out[OIdx] = (float)out[OIdx] + (float)in[IIdx] * (float)filter[WIdx];
                    #elif CALCULATE_ACCURACY==HIGH_ACCURACY
                    out[OIdx] = (double)out[OIdx] + (double)in[IIdx] * (double)filter[WIdx];
                    #endif
                    
                }
            }
        }
    }
}

/*Function: conv3D
    Three-dimensional convolution

    Parameters:
        in - input 
        out - output
        Ic - input channel
        Iw - input width
        Ih - input height
        filter - filter(K * Iw * Ic * Oc)
        K - size of filter
        S - stride
        bias - bias
        Oc - Output channel

    Note: NO padding.
*/
void conv3D(DATA_TYPE *in, DATA_TYPE *out, int Ic, int Iw, int Ih, int Id, DATA_TYPE *filter, int K, int S, DATA_TYPE *bias, int Oc){
    //printf("conv3D...:D\n");
    int no, ni, x, y, z, kx, ky, kz, iw, ih, id, OIdx, IIdx, WIdx, Oh, Ow, Od;
    Ow = (Iw - K)/S + 1;
    Oh = (Ih - K)/S + 1;
    Od = (Id - K)/S + 1;
    for(no = 0; no < Oc; no++){ // output channel(the number of filters)
        for(ni = 0; ni < Ic; ni++){ // input channel
            for(z = 0; z < Od; z++){ // output depth
                for(y = 0; y < Oh; y++){ // output height
                    for(x = 0; x < Ow; x++){ // output width                     
                        OIdx = no * Ow * Oh * Od + z * Ow * Oh + y * Ow + x;   
                        if(ni == 0){
                            // output(no, x, y) = bias(no)
                            out[OIdx] = bias[no];
                        } 
                        for(kz = 0; kz < K; kz++){ // filter d
                            for(ky = 0; ky < K; ky++){ // filter h
                                for(kx = 0; kx < K; kx++){// filter w
                                    iw = kx + x*S;
                                    ih = ky + y*S;
                                    id = kz + z*S;                            
                                    // output(no, x, y, z) = input(ni, iw, ih, id) * weight(ni, no, kx, ky, kz)
                                    IIdx = ni * Iw * Ih * Id + id * Ih * Iw + ih * Iw + iw;
                                    WIdx = no * K * K * K * Ic + ni * K * K * K + kz * K * K + ky * K + kx;
                                    #if CALCULATE_ACCURACY==NORMAL
                                    out[OIdx] += in[IIdx] * filter[WIdx];
                                    #elif CALCULATE_ACCURACY==LOW_ACCURACY
                                    out[OIdx] = (float)out[OIdx] + (float)in[IIdx] * (float)filter[WIdx];
                                    #elif CALCULATE_ACCURACY==HIGH_ACCURACY
                                    out[OIdx] = (double)out[OIdx] + (double)in[IIdx] * (double)filter[WIdx];
                                    #endif
                                }
                            }
                        }                
                    }
                }
            }
        }
    }
}
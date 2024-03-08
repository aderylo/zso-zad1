#include "benchmark.h"

/* Function: list_init
    Initialize list with data.

    Parameters:
        blksize - Size of memory to be initialized
        memblk - Pointer to memory block
        seed - CANNOT be determined at compiler time, decide the data to fill the memory

    Return:
        Pointer to the head of list.

    Note: 
        data in different dimentions.
*/
list_node *
list_init(uint32_t blksize, list_node *memblk, sint16_t seed){
    //printf("list_init...OoO\n");
    /*calculate the number of pointers*/
    uint32_t item_size  = 16 + sizeof(struct list_data_s);// sizeof(list_node) + sizeof(list_info)
    uint32_t item_num = (blksize / item_size) - 1; // list head 

    list_node *nodes_end = memblk + item_num;

    list_data *datablk = (list_data *)(nodes_end);
    list_data *data_end = datablk + item_num;

    uint32_t i;
    list_node *list = memblk;
    list_data info;

    /*create head*/

    /*head*/
    list->next = NULL;
    list->info = datablk;
    list->info->idx = 0; /*real item's idx is in (0, 0xffffffff)*/
    /*for(int k = 0; k < DATA_DIM; k++){
        list->info->data[k] = 0;
    }*/
    #if DATA_DIM==1
    for(int l = 0; l < DIM_X; l++){
        list->info->data[l] = 0;
    }
    #elif DATA_DIM==2
    for(int l = 0; l < DIM_X; l++){
        for(int m = 0; m < DIM_Y; m++){
            list->info->data[l][m] = 0;
        }
    }
    #elif DATA_DIM==3
    for(int l = 0; l < DIM_X; l++){
        for(int m = 0; m < DIM_Y; m++){
            for(int n = 0; n < DIM_Z; n++){
                list->info->data[l][m][n] = 0;
            }
        }
    }
    #endif
    
    memblk++;
    datablk++;
    uint32_t tmp = 0;
    /*initial idx and data*/
    #if TEST_TYPE == INT_TYPE
    for(i = 1; i <= item_num; i++){
        tmp = (seed ^ i);
        #if DATA_DIM==1
        for(int l = 0; l < DIM_X; l++){
            info.data[l] = (tmp) + l;
        }
        #elif DATA_DIM==2
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                info.data[l][m] = (tmp) + (l ^ m);
            }
        }
        #elif DATA_DIM==3
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                for(int n = 0; n < DIM_Z; n++){
                    info.data[l][m][n] = (tmp) + (l ^ m ^ n);
                }
            }
        }
        #endif
    #else
    for(i = 1; i <= item_num; i++){
        tmp = (seed ^ i);

        #if DATA_DIM==1
        for(int l = 0; l < DIM_X; l++){
            info.data[l] = (((tmp << 16) | tmp) + l)/((seed << 10) * 1.3);
        }
        #elif DATA_DIM==2
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                info.data[l][m] = (((tmp << 16) | tmp) + (l ^ m))/((seed << 10) * 1.3);
            }
        }
        #elif DATA_DIM==3
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                for(int n = 0; n < DIM_Z; n++){
                    info.data[l][m][n] = (((tmp << 16) | tmp) + (l ^ m ^ n))/((seed << 10) * 1.3);
                }
            }
        }
        #endif
    #endif
        info.idx = i;
        list_insert(list, &info, &memblk, &datablk, nodes_end, data_end);
    }
    return list;
}

/* Function: list_insert
    Insert an item to the list

    Parameters:
        insert_point - insert the item next the insert_point
        info - data and idx
        memblk - pointer for the list node head
        datablk - pointer for the list data head
        memblk_end - end of region for list nodes
        datablk_end - end of region for list data

    Returns:
        Pointer to new item

*/
list_node *
list_insert(list_node *insert_point, list_data *info, list_node **memblk, list_data **datablk, list_node *nodes_end, list_data *data_end){
    list_node *new_item;
    if((*memblk + 1) >= nodes_end || (*datablk + 1) >= data_end){
        return NULL;
    }
    new_item = *memblk;
    (*memblk)++;
    new_item->next = insert_point->next;
    insert_point->next = new_item;

    new_item->info = (*datablk);
    (*datablk)++;
    //new_item->info->data = info->data;
    /*for(int k = 0; k < DATA_DIM; k++){
        new_item->info->data[k] = info->data[k];
    }*/
    #if DATA_DIM==1
        for(int l = 0; l < DIM_X; l++){
            new_item->info->data[l] = info->data[l];
        }
    #elif DATA_DIM==2
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                new_item->info->data[l][m] = info->data[l][m];
            }
        }
    #elif DATA_DIM==3
        for(int l = 0; l < DIM_X; l++){
            for(int m = 0; m < DIM_Y; m++){
                for(int n = 0; n < DIM_Z; n++){
                    new_item->info->data[l][m][n] = info->data[l][m][n];
                }
            }
        }
    #endif

    new_item->info->idx = info->idx;
    return new_item;
}

/*Function: cmp_idx_value
    cmp two list_info according to idx or data value

    Returns:
        1 - a > b
        0 - a == b
        -1 - a < b
    
    Note:
        for two(three)-dimentional data, the weight is decremented.
*/
int cmp_idx_value(list_data *a, list_data *b, uint8_t cmp){
    if(cmp == DATA_CMP){
        int l=0, m=0, n=0;
            /*
            while(k != DATA_DIM){
                if(a->data[k] > b->data[k]) {return 1;}
                else if(a->data[k] < b->data[k]) {return -1;}
                else {k++;}
            }
            return 0;
            */
            #if DATA_DIM==1
            while(l != DIM_X){
                #if CALCULATE_ACCURACY == NORMAL

                if(a->data[l] > b->data[l]) {return 1;}
                else if(a->data[l] < b->data[l]) {return -1;}
                else {l++;}

                #elif CALCULATE_ACCURACY == LOW_ACCURACY
                if((float)a->data[l] > (float)b->data[l]) {return 1;}
                else if((float)a->data[l] < (float)b->data[l]) {return -1;}
                else {l++;}

                #elif CALCULATE_ACCURACY == HIGH_ACCURACY
                if((double)a->data[l] > (double)b->data[l]) {return 1;}
                else if((double)a->data[l] < (double)b->data[l]) {return -1;}
                else {l++;}

                #endif
            }
            l = 0;
            return 0;
            #elif DATA_DIM == 2
            while(l != DIM_X){
                while(m != DIM_Y){
                    #if CALCULATE_ACCURACY == NORMAL

                    if(a->data[l][m] > b->data[l][m]) {return 1;}
                    else if(a->data[l][m] < b->data[l][m]) {return -1;}
                    else {m++;}

                    #elif CALCULATE_ACCURACY == LOW_ACCURACY
                    if((float)a->data[l][m] > (float)b->data[l][m]) {return 1;}
                    else if((float)a->data[l][m] < (float)b->data[l][m]) {return -1;}
                    else {m++;}

                    #elif CALCULATE_ACCURACY == HIGH_ACCURACY
                    if((double)a->data[l][m] > (double)b->data[l][m]) {return 1;}
                    else if((double)a->data[l][m] < (double)b->data[l][m]) {return -1;}
                    else {m++;}

                    #endif
                    
                }
                l++;
                m=0;
            }
            l = 0, m = 0;
            return 0;
            #elif DATA_DIM == 3
            while(l != DIM_X){
                while(m != DIM_Y){
                    while(n != DIM_Z){
                        #if CALCULATE_ACCURACY == NORMAL

                        if(a->data[l][m][n] > b->data[l][m][n]) {return 1;}
                        else if(a->data[l][m][n] < b->data[l][m][n]) {return -1;}
                        else {n++;}

                        #elif CALCULATE_ACCURACY == LOW_ACCURACY

                        if((float)a->data[l][m][n] > (float)b->data[l][m][n]) {return 1;}
                        else if((float)a->data[l][m][n] < (float)b->data[l][m][n]) {return -1;}
                        else {n++;}

                        #elif CALCULATE_ACCURACY == HIGH_ACCURACY

                        if((double)a->data[l][m][n] > (double)b->data[l][m][n]) {return 1;}
                        else if((double)a->data[l][m][n] < (double)b->data[l][m][n]) {return -1;}
                        else {n++;}

                        #endif
                    }
                    m++;
                    n=0;    
                }
                l++;
                m=0;
                n=0;
            }
            l = 0, m = 0, n = 0;
            return 0;
            #endif
    }
    else if(cmp == IDX_CMP){
        if(a->idx > b->idx){
            return 1;
        }
        else if(a->idx == b->idx){
            return 0;
        }
        else{
            return -1;
        }
        
    }
}

/*Function: list_mergesort
    sort the list according to the data without recursion
    
    Description:
        since this is aimed at embedded, choose to use iterative rather than recursion.
        This sort can either return the list to original order according to idx, or according to data.

    Parameters:
        list - list to be sorted
        cmp - cmp function to use

    Returns:
        Pointer to new list

    Note:
        CAN modify where the list starts

*/
list_node *
list_mergesort(list_node *list, uint8_t cmp){
    //printf("list_mergesort...>-<\n");
    list_node *left, *right, *add, *ret;
    uint32_t insize, nmerges, lsize, rsize, i;
    insize = 1;

    while(1){
        left = list;
        list = NULL;
        ret = NULL;
        nmerges = 0;

        while(left){
            nmerges ++;
            right = left;
            lsize = 0;
            rsize = 0;
            // left and right sublist
            for(i = 0; i < insize; i++){
                lsize ++;
                right = right->next;
                if(!right){ // right == NULL
                    break;
                }
            }
            rsize = insize;

            while(lsize > 0 || (rsize > 0 && right)){ // left or right is not empty
                if(lsize == 0){// left is empty
                    add = right;
                    right = right -> next;
                    rsize --;
                }
                else if(rsize == 0 || !right){// right is empty
                    add = left;
                    left = left -> next;
                    lsize --;
                }
                else if(cmp_idx_value(right->info, left->info, cmp) < 0){// right < left
                    add = right;
                    right = right->next;
                    rsize --;
                }
                else{ // right >= left
                    add = left;
                    left = left->next;
                    lsize --;
                }

                // add to the new list--ret
                if(ret){
                    ret->next = add;
                }
                else{
                    list = add; // head
                }
                ret = add;
            }
            /* left strides "insize", right too; i.e. the left of "right" are sorted*/
            left = right; 
        }
        ret->next = NULL;
        
        if(nmerges <= 1){
            return list;
        }
        insize *= 2;
    }
}

/*Function: list_quicksort
    sort the list according to the data without recursion
    
    Description:
        since this is aimed at embedded, choose to use iterative rather than recursion.
        This sort can either return the list to original order according to idx, or according to data.

    Parameters:
        list - list to be sorted
        cmp - cmp function to use

    Returns:
        Pointer to new list

    Note:
        CAN modify where the list starts

*/
/*
list_node *
list_quicksort(list_node *list){

}
*/
/*Function: list_search
    search for an item by idx or data value

    Parameters:
        list - list head
        info - idx & data
        key - search based which info 

    Returns:
        Pointer to the item or NULL(NOT FOUND)
*/
list_node *
list_search(list_node *list, list_data *info, uint8_t key){
    //printf("list_search....^-^\n");
    if(key == IDX_FIND){
        while(list && (cmp_idx_value(list->info, info, IDX_CMP) != 0)){
            list = list->next;
        }
        return list;
    }
    else{
        while(list && (cmp_idx_value(list->info, info, DATA_CMP) != 0)){
            list = list->next;
        }
        return list;
    }
        
}

/*Function: list_search_all
    find out all items which meet the condition

    Parameters;
        list - list head
        info - data
        
    Returns:
        pointer to the result array
*/
uint16_t *
list_search_all(list_node *list, list_data *info){
    //printf("list_search_all...@-@\n");
    int i = 0;
    /*
    if(list && (cmp_idx_value(list->info, info, DATA_CMP) != 0)){
        list = list->next;
    }
    else{
        result[i++] = list->info->idx;
    }
    */
    while(list!=NULL){
        if(list && (cmp_idx_value(list->info, info, DATA_CMP) != 0)){
            list = list->next;
        }
        else if(list){
            result[i++] = list->info->idx;
            list = list->next;
        }
    }
}
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include "utils.h"

#include <cmath>

/*
 * Binary utils
 */

uint64_t getMask(uint64_t length){
    return pow(2, length)-1;
}

void bin(unsigned n)
{
    if (n > 1) {
        bin(n >> 1);
    }
    printf("%d", n & 1);
}


/*
 * Address managment
 */

uint64_t getPageAddress(uint64_t virtual_address, int depth){
    if (depth > TABLES_DEPTH){
        return error(SYS_ERR, "get address, invalid depth");
    }
    uint64_t page_width_mask = getMask(OFFSET_WIDTH);
    uint64_t shifted_page_mask = page_width_mask << (TABLES_DEPTH - depth) * OFFSET_WIDTH; // TODO check it
    uint64_t ret = (virtual_address & shifted_page_mask) >> (TABLES_DEPTH - depth) * OFFSET_WIDTH;
    return ret;
}

uint64_t getOffset(uint64_t virtual_address){
    int offset_width_mask = getMask(OFFSET_WIDTH);
    return virtual_address & offset_width_mask;
}

uint64_t getPageRoute(uint64_t virtual_address){
    return virtual_address >> OFFSET_WIDTH;
}

//void addressDebuger(uint64_t virtual_address){
//    std::cout << "Virtual Address: " <<  virtual_address << " ";
//    bin(virtual_address);
//    std::cout << std::endl;
//
//    std::cout << "Level  |  value  |  binary" << std::endl;
//    for (int i = 0; i < TABLES_DEPTH; ++i) {
//        uint64_t addr = getPageAddress(virtual_address, i);
//        std::cout << "  "<< i << "    |    " << addr << "    |  ";
//        bin(addr);
//        std::cout << std::endl;
//    }
//    uint64_t offset = getOffset(virtual_address);
//    std::cout << "Offset |    " << offset << "    |  ";
//    bin(offset);
//    std::cout <<  std::endl;
//}
//
//void frameDebug(uint64_t frame_index){
//    std::cout << "Frame: " << frame_index << std::endl;
//    for (int i = 0; i < PAGE_SIZE; ++i) {
//        word_t data;
//        PMread(frame_index * PAGE_SIZE + i, &data);
//        std::cout << data << std::endl;
//    }
//}

int legalAddress(uint64_t address){
    return address < 0 || address >= VIRTUAL_MEMORY_SIZE;
}

/*
 * Page Handlers
 */

void clearFrame(uint64_t frame_index){
    for (int i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame_index * PAGE_SIZE + i, 0);
    }
}


typedef struct {
    // global data
    uint64_t global_opt1_empty_frame_index;

    word_t global_opt2_max_frame_index;

    int global_opt3_page_score;
    uint64_t global_opt3_frame_to_evict;
    uint64_t global_opt3_page_to_evict;
    uint64_t global_opt3_parent_of_page_to_evict;

    // current data
    uint64_t parent_frame;
    uint64_t parent_offset;
    // uint64_t current_route;

    // data supplied ahead
    word_t blocked_frame;
    uint64_t page_swapped_in;


} TraverseData;

bool pageTableTraverse(word_t frame_index, int depth, TraverseData *data, uint64_t current_route){

    if (frame_index > data->global_opt2_max_frame_index){
        data->global_opt2_max_frame_index = frame_index;
    }

    if (depth == TABLES_DEPTH){
        int abs_diff = abs(data->page_swapped_in, current_route);
        int page_score = min(NUM_PAGES - abs_diff, abs_diff);
        if (page_score > data->global_opt3_page_score){
            data->global_opt3_page_score = page_score;
            data->global_opt3_frame_to_evict = frame_index;
            data->global_opt3_parent_of_page_to_evict = data->parent_frame * PAGE_SIZE + data->parent_offset;
            data->global_opt3_page_to_evict = current_route;
        }
        return false;
    }


    current_route = current_route << OFFSET_WIDTH;
    bool isTableEmpty = true;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        word_t value;
        PMread(frame_index * PAGE_SIZE + i, &value);
        if (value != 0){
            isTableEmpty = false;
            data->parent_frame = frame_index;
            data->parent_offset = i;

            bool hasEmptyChildren = pageTableTraverse(value, depth + 1, data, current_route + i);

            if (hasEmptyChildren){
                return true;
            }
        }
    }

    if (isTableEmpty && frame_index != data->blocked_frame){
        data->global_opt1_empty_frame_index = frame_index;
        PMwrite(data->parent_frame * PAGE_SIZE + data->parent_offset , 0); // Removing the referance
        return true;
    }
    return false;
}



word_t getFrame(uint64_t page_swapped_in, uint64_t parent_frame){
    // Traversing the graph and extracting parameters
    TraverseData data;

    data.global_opt1_empty_frame_index = 0;
    data.global_opt2_max_frame_index = 0;
    data.global_opt3_page_score = 0;
    data.global_opt3_frame_to_evict = 0;
    data.global_opt3_page_to_evict = 0;
    data.global_opt3_parent_of_page_to_evict = 0;

    data.parent_frame = 0;
    data.parent_offset = 0;
    data.blocked_frame = parent_frame;
    data.page_swapped_in = page_swapped_in;

    bool has_empty_table = pageTableTraverse(0, 0, &data, 0);

    // Option 1 - look for an empty table
    if (has_empty_table){
        return data.global_opt1_empty_frame_index;
    }

    // Option 2 - check max frame index (calculated in option 1)
    if (data.global_opt2_max_frame_index + 1 < NUM_FRAMES){
        return data.global_opt2_max_frame_index + 1;
    }

    // Option 3
    PMwrite(data.global_opt3_parent_of_page_to_evict, 0);
    PMevict(data.global_opt3_frame_to_evict, data.global_opt3_page_to_evict);
    return data.global_opt3_frame_to_evict;
}


word_t readPage(uint64_t page_number, uint64_t row_number, uint64_t page_swapped_in, uint64_t parent_frame, int depth){
    word_t address;
    PMread(page_number * PAGE_SIZE + row_number, &address);

    if (address == 0){
        word_t frame_number = getFrame(page_swapped_in, parent_frame);

        if (depth == TABLES_DEPTH - 1){
            PMrestore(frame_number, page_swapped_in);
        }
        else{
            clearFrame(frame_number);
        }
        PMwrite(page_number * PAGE_SIZE + row_number, frame_number);
        return frame_number;
    }
    return address;
}



/*
 * API
 */

void VMinitialize(){
    clearFrame(0);
}

int VMread(uint64_t virtualAddress, word_t* value){
    if (legalAddress(virtualAddress)){
        return 0;
    }

    uint64_t current_page = 0;
    for (int i=0; i< TABLES_DEPTH; i++){
        uint64_t row_number = getPageAddress(virtualAddress, i);
        current_page = readPage(current_page, row_number, getPageRoute(virtualAddress), current_page, i);
    }
    uint64_t offset = getOffset(virtualAddress);
    PMread(current_page * PAGE_SIZE + offset, value);

    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value){
    if (legalAddress(virtualAddress)){
        return 0;
    }
    uint64_t current_page = 0;
    for (int i=0; i< TABLES_DEPTH; i++){
        uint64_t row_number = getPageAddress(virtualAddress, i);
        current_page = readPage(current_page, row_number, getPageRoute(virtualAddress), current_page, i);
    }
    uint64_t offset = getOffset(virtualAddress);
    PMwrite(current_page * PAGE_SIZE + offset, value);

    return 1;
}




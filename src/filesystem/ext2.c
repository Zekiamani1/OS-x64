#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../stdlib/string.h"
#include "../driver/disk.h"
#include "ext2.h"
static struct EXT2Superblock superblock;
static struct EXT2BlockGroupDescriptorTable bgd_table;
const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/* =================== FILESYSTEM SIGNATURE (fs_signature) =================== */
bool is_empty_storage(void) {
    uint8_t signature_block[BLOCK_SIZE];
    read_blocks(signature_block, 0, 1);
    
    // Check if the signature matches  EXT2 filesystem signature
    if(memcmp(signature_block,fs_signature,BLOCK_SIZE)){
        return true;
    }
    
    struct BlockBuffer buf;
    read_blocks(&buf, 1, 1);
    memcpy(&superblock, buf.buf, sizeof(superblock));
    return false;
}

/* =================== INODE NUMBER UTILITIES =================== */

uint32_t inode_to_bgd(uint32_t inode){
    uint32_t bgd = (inode - 1) / INODES_PER_GROUP;
    return bgd;
}
// → Converts inode number to EXT2BlockGroupDescriptorTable index

uint32_t inode_to_local(uint32_t inode){
    return (inode - 1) % INODES_PER_GROUP; 
}
// → Converts inode number to local index in EXT2InodeTable.table[]

/* =================== EXT2DirectoryEntry OPERATIONS =================== */

char *get_entry_name(void *entry){
    //strukturnya tuh, [EXT2DirectoryEntry][name][padding], biar ukuran string name tuh bisa berubah ubah, ga statis
    return (char *) entry + sizeof(struct EXT2DirectoryEntry);
}    

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset){
    return (struct EXT2DirectoryEntry *) ((uint8_t *)ptr +offset);
} 
// → Gets EXT2DirectoryEntry at specific offset in directory block

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry){
    return (struct EXT2DirectoryEntry *) ((uint8_t *) entry +entry->rec_len);
} 
// → Navigates to next EXT2DirectoryEntry using rec_len

uint16_t get_entry_record_len(uint8_t name_len){
    return (sizeof(struct EXT2DirectoryEntry) + name_len + 3) & ~3;
} 
// → Calculates EXT2DirectoryEntry.rec_len (struct + filename + padding)

uint32_t get_dir_first_child_offset(void *ptr) {
    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)ptr;
    struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);
    return (uint8_t *)get_next_directory_entry(dotdot) - (uint8_t *)ptr;
}
// → Finds first non-"." non-".." entry in directory

/* =================== EXT2Inode OPERATIONS =================== */

void sync_node(struct EXT2Inode *node, uint32_t inode){
    // Calculate which block group contains this inode
    uint32_t group = inode_to_bgd(inode);

    // Use global block group descriptor table
    struct EXT2BlockGroupDescriptorTable *bgdt = &bgd_table;

    // Calculate the inode's position within the inode table
    uint32_t local_inode = inode_to_local(inode); // 0-based index in group
    uint32_t inode_table_block = bgdt->table[group].bg_inode_table;
    uint32_t inode_block_offset = local_inode / INODES_PER_TABLE;
    uint32_t inode_index_in_block = local_inode % INODES_PER_TABLE;

    // Read the specific inode table block
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, inode_table_block + inode_block_offset, 1);

    // Update the inode in the table
    inode_table.table[inode_index_in_block] = *node;

    // Write the updated inode table block back to disk
    write_blocks(&inode_table, inode_table_block + inode_block_offset, 1);
} 
// → Writes EXT2Inode to correct position in EXT2InodeTable on disk

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode){
    uint8_t dir_buffer[BLOCK_SIZE] = {0};

    struct EXT2DirectoryEntry *self = get_directory_entry(dir_buffer, 0);
    self->inode = inode;
    self->file_type = EXT2_FT_DIR;
    self->name_len = 1;
    self->rec_len = 12;
    char *self_name = get_entry_name(self);
    self_name[0] = '.';

    struct EXT2DirectoryEntry *parent = get_next_directory_entry(self);
    parent->inode = parent_inode;
    parent->file_type = EXT2_FT_DIR;
    parent->name_len = 2;
    parent->rec_len = 12;
    char *parent_name = get_entry_name(parent);
    parent_name[0] = '.';
    parent_name[1] = '.';

    struct EXT2DirectoryEntry *empty_entry = get_next_directory_entry(parent);
    empty_entry->inode = 0;
    empty_entry->rec_len = BLOCK_SIZE - 24; 

    node->i_size = BLOCK_SIZE;
    node->i_blocks = 1; 
    node->i_mode = EXT2_S_IFDIR;
    
    allocate_node_blocks(dir_buffer, node, inode_to_bgd(inode));
        
    sync_node(node, inode);
}
// → Creates "." and ".." entries in new directory

bool is_directory_empty(uint32_t inode) {
    struct EXT2Inode node;
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    
    struct BlockBuffer buf;
    uint32_t inode_block = bgd_table.table[bgd_idx].bg_inode_table + (local_idx / INODES_PER_TABLE);
    uint32_t inode_offset = (local_idx % INODES_PER_TABLE) * INODE_SIZE;
    
    read_blocks(&buf, inode_block, 1);
    memcpy(&node, buf.buf + inode_offset, sizeof(node));
    
    read_blocks(&buf, node.i_block[0], 1);
    uint32_t first_child_offset = get_dir_first_child_offset(buf.buf);
    struct EXT2DirectoryEntry *first_child = get_directory_entry(buf.buf, first_child_offset);
    
    return first_child->inode == 0;
}
// → Checks if directory only contains "." and ".." entries

/* =================== BLOCK GROUP DESCRIPTOR & BITMAP OPERATIONS =================== */

uint32_t allocate_node(void){
    int i=0;
    while(bgd_table.table[i].bg_free_inodes_count==0){
        i++;
        if(i==GROUPS_COUNT) return 0;
    }
    struct EXT2BlockGroupDescriptor *bgd=&bgd_table.table[i];
    uint8_t inode_bitmap[BLOCK_SIZE];
    read_blocks(inode_bitmap, bgd->bg_inode_bitmap, 1);
    int j=0;
    while(inode_bitmap[j]==0xFF){
        j++;
    }
    int k=0;
    uint8_t mask=1;
    while((inode_bitmap[j]&mask)){
        k++;
        mask=mask<<1;
    }
    inode_bitmap[j]|=mask;
    bgd->bg_free_inodes_count--;
    write_blocks(inode_bitmap,bgd->bg_inode_bitmap,1);
    write_blocks(&bgd_table, 2, 1);
    uint32_t local_inode = (j * 8) + k;
    uint32_t global_inode = (i * INODES_PER_GROUP) + local_inode + 1;
    return global_inode;
}        
// → Finds free inode in bitmap, returns new inode number

uint32_t allocate_free_block(uint32_t prefered_bgd){
    // Range check for prefered_bgd
    if (prefered_bgd >= GROUPS_COUNT) {
        prefered_bgd = 0;
    }

    int i;
    if(bgd_table.table[prefered_bgd].bg_free_blocks_count > 0) {
        i = prefered_bgd;
    } else {
        i = 0;
        while(bgd_table.table[i].bg_free_blocks_count == 0){
            i++;
            if(i == GROUPS_COUNT) return 0;
        }
    }

    struct EXT2BlockGroupDescriptor *bgd = &bgd_table.table[i];
    uint8_t block_bitmap[BLOCK_SIZE];
    read_blocks(block_bitmap, bgd->bg_block_bitmap, 1);

    int j = 0;
    while(j < BLOCK_SIZE && block_bitmap[j] == 0xFF){
        j++;
    }
    if (j >= BLOCK_SIZE) {
        // No free block found in bitmap
        return 0;
    }

    int k = 0;
    uint8_t mask = 1;
    while((block_bitmap[j] & mask) && k < 8){
        k++;
        mask = mask << 1;
    }
    if (k >= 8) {
        // No free bit found in this byte
        return 0;
    }

    block_bitmap[j] |= mask;
    bgd->bg_free_blocks_count--;

    write_blocks(block_bitmap, bgd->bg_block_bitmap, 1);
    write_blocks(&bgd_table, 2, 1);

    // Calculate global block number
    uint32_t local_block = (j * 8) + k;
    uint32_t global_block = (i * BLOCKS_PER_GROUP) + local_block;

    return global_block;
}

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd){
    uint32_t blocks_needed = (node->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t blocks_allocated = 0;
    // Use global bgd_table instead of local bgdt
    
    // Allocate direct blocks (0-11)
    for (int i = 0; i < 12 && blocks_allocated < blocks_needed; i++) {
        node->i_block[i] = allocate_free_block(prefered_bgd);
        uint32_t remaining = node->i_size - (blocks_allocated * BLOCK_SIZE);
        uint32_t to_write = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        write_blocks((uint8_t*)ptr + (blocks_allocated * BLOCK_SIZE), node->i_block[i], 1);
        if (to_write < BLOCK_SIZE) {
            // Zero out the rest of the block if partial write
            uint8_t zero_buf[BLOCK_SIZE] = {0};
            if (to_write > 0) {
                memcpy(zero_buf, (uint8_t*)ptr + (blocks_allocated * BLOCK_SIZE), to_write);
            }
            write_blocks(zero_buf, node->i_block[i], 1);
        }
        blocks_allocated++;
    }
    
    // If more blocks needed, use single indirect (block 12)
    if (blocks_allocated < blocks_needed) {
        // Allocate indirect block
        node->i_block[12] = allocate_free_block(prefered_bgd);

        // Create array of block pointers (128 pointers in 512-byte block)
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];

        // Fill indirect block with data block pointers
        uint32_t indirect_index = 0;
        while (blocks_allocated < blocks_needed && indirect_index < (BLOCK_SIZE / sizeof(uint32_t))) {
            // Allocate data block
            indirect_blocks[indirect_index] = allocate_free_block(prefered_bgd);

            // Write data to the allocated block
            uint32_t remaining = node->i_size - (blocks_allocated * BLOCK_SIZE);
            uint32_t to_write = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
            write_blocks((uint8_t*)ptr + (blocks_allocated * BLOCK_SIZE), indirect_blocks[indirect_index], 1);
            if (to_write < BLOCK_SIZE) {
                uint8_t zero_buf[BLOCK_SIZE] = {0};
                if (to_write > 0) {
                    memcpy(zero_buf, (uint8_t*)ptr + (blocks_allocated * BLOCK_SIZE), to_write);
                }
                write_blocks(zero_buf, indirect_blocks[indirect_index], 1);
            }
            blocks_allocated++;
            indirect_index++;
        }

        // Write the indirect block to disk
        write_blocks(indirect_blocks, node->i_block[12], 1);
    }
    
    // Update blocks count in inode
    node->i_blocks = blocks_allocated;
} 
// → Allocates blocks and populates EXT2Inode.i_block[] array

void deallocate_node(uint32_t inode) {
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    
    struct EXT2Inode node;
    struct BlockBuffer buf;
    uint32_t inode_block = bgd_table.table[bgd_idx].bg_inode_table + (local_idx / INODES_PER_TABLE);
    uint32_t inode_offset = (local_idx % INODES_PER_TABLE) * INODE_SIZE;
    
    read_blocks(&buf, inode_block, 1);
    memcpy(&node, buf.buf + inode_offset, sizeof(node));
    
    deallocate_blocks(node.i_block, node.i_blocks);
    
    memset(&node, 0, sizeof(node));
    memcpy(buf.buf + inode_offset, &node, sizeof(node));
    write_blocks(&buf, inode_block, 1);
    
    struct BlockBuffer bitmap;
    read_blocks(&bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
    uint32_t byte_idx = local_idx / 8;
    uint32_t bit_idx = local_idx % 8;
    bitmap.buf[byte_idx] &= ~(1 << bit_idx);
    write_blocks(&bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
    
    bgd_table.table[bgd_idx].bg_free_inodes_count++;
    superblock.s_free_inodes_count++;
    
    memcpy(buf.buf, &bgd_table, sizeof(bgd_table));
    write_blocks(&buf, 2, 1);
    
    memcpy(buf.buf, &superblock, sizeof(superblock));
    write_blocks(&buf, 1, 1);
}
// → Marks inode as free in bitmap

uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, 
                         uint32_t depth, uint32_t *last_bgd, bool bgd_loaded) {
    if (depth == 0) {
        for (uint32_t i = 0; i < blocks; i++) {
            if (locations[i] == 0) continue;
            
            uint32_t block_bgd = locations[i] / BLOCKS_PER_GROUP;
            if (!bgd_loaded || block_bgd != *last_bgd) {
                if (bgd_loaded) {
                    write_blocks(bitmap, bgd_table.table[*last_bgd].bg_block_bitmap, 1);
                }
                read_blocks(bitmap, bgd_table.table[block_bgd].bg_block_bitmap, 1);
                *last_bgd = block_bgd;
                bgd_loaded = true;
            }
            
            uint32_t local_block = locations[i] % BLOCKS_PER_GROUP;
            uint32_t byte_idx = local_block / 8;
            uint32_t bit_idx = local_block % 8;
            bitmap->buf[byte_idx] &= ~(1 << bit_idx);
            
            bgd_table.table[block_bgd].bg_free_blocks_count++;
            superblock.s_free_blocks_count++;
        }
        return *last_bgd;
    }
    
    struct BlockBuffer indirect;
    for (uint32_t i = 0; i < blocks; i++) {
        if (locations[i] == 0) continue;
        
        read_blocks(&indirect, locations[i], 1);
        uint32_t *indirect_ptrs = (uint32_t *)indirect.buf;
        uint32_t entries = BLOCK_SIZE / sizeof(uint32_t);
        
        *last_bgd = deallocate_block(indirect_ptrs, entries, bitmap, depth - 1, last_bgd, bgd_loaded);
        bgd_loaded = true;
        
        uint32_t block_bgd = locations[i] / BLOCKS_PER_GROUP;
        if (block_bgd != *last_bgd) {
            write_blocks(bitmap, bgd_table.table[*last_bgd].bg_block_bitmap, 1);
            read_blocks(bitmap, bgd_table.table[block_bgd].bg_block_bitmap, 1);
            *last_bgd = block_bgd;
        }
        
        uint32_t local_block = locations[i] % BLOCKS_PER_GROUP;
        uint32_t byte_idx = local_block / 8;
        uint32_t bit_idx = local_block % 8;
        bitmap->buf[byte_idx] &= ~(1 << bit_idx);
        
        bgd_table.table[block_bgd].bg_free_blocks_count++;
        superblock.s_free_blocks_count++;
    }
    
    return *last_bgd;
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *locations = (uint32_t *)loc;
    struct BlockBuffer bitmap;
    uint32_t last_bgd = 0;
    bool bgd_loaded = false;
    
    uint32_t direct_blocks = blocks < 12 ? blocks : 12;
    if (direct_blocks > 0) {
        last_bgd = deallocate_block(locations, direct_blocks, &bitmap, 0, &last_bgd, bgd_loaded);
        bgd_loaded = true;
    }
    
    if (blocks > 12) {
        last_bgd = deallocate_block(&locations[12], 1, &bitmap, 1, &last_bgd, bgd_loaded);
        bgd_loaded = true;
    }
    
    if (blocks > 12 + BLOCK_SIZE / sizeof(uint32_t)) {
        last_bgd = deallocate_block(&locations[13], 1, &bitmap, 2, &last_bgd, bgd_loaded);
    }
    
    if (bgd_loaded) {
        write_blocks(&bitmap, bgd_table.table[last_bgd].bg_block_bitmap, 1);
    }
    
    struct BlockBuffer buf;
    memcpy(buf.buf, &bgd_table, sizeof(bgd_table));
    write_blocks(&buf, 2, 1);
    
    memcpy(buf.buf, &superblock, sizeof(superblock));
    write_blocks(&buf, 1, 1);
}

void read_node_blocks(struct EXT2Inode *node, void *buffer) {
    uint32_t node_blocks = (node->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t bytes_copied = 0;
    uint32_t blocks_read = 0;
    struct BlockBuffer buf;
    
    // Read direct blocks (0-11)
    for (uint32_t i = 0; i < 12 && blocks_read < node_blocks; i++) {
        if (node->i_block[i] == 0) continue;
        
        read_blocks(&buf, node->i_block[i], 1);
        uint32_t bytes_to_copy = (node->i_size - bytes_copied) < BLOCK_SIZE ? 
                                 (node->i_size - bytes_copied) : BLOCK_SIZE;
        memcpy((uint8_t*)buffer + bytes_copied, buf.buf, bytes_to_copy);
        bytes_copied += bytes_to_copy;
        blocks_read++;
    }
    
    // Read single indirect blocks if needed
    if (blocks_read < node_blocks && node->i_block[12] != 0) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, node->i_block[12], 1);
        
        for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_read < node_blocks; j++) {
            if (indirect_blocks[j] == 0) continue;
            
            read_blocks(&buf, indirect_blocks[j], 1);
            uint32_t bytes_to_copy = (node->i_size - bytes_copied) < BLOCK_SIZE ? 
                                     (node->i_size - bytes_copied) : BLOCK_SIZE;
            memcpy((uint8_t*)buffer + bytes_copied, buf.buf, bytes_to_copy);
            bytes_copied += bytes_to_copy;
            blocks_read++;
        }
    }
    
    // Read double indirect blocks if needed
    if (blocks_read < node_blocks && node->i_block[13] != 0) {
        uint32_t double_indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(double_indirect_blocks, node->i_block[13], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_read < node_blocks; i++) {
            if (double_indirect_blocks[i] == 0) continue;
            
            uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(indirect_blocks, double_indirect_blocks[i], 1);
            
            for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_read < node_blocks; j++) {
                if (indirect_blocks[j] == 0) continue;
                
                read_blocks(&buf, indirect_blocks[j], 1);
                uint32_t bytes_to_copy = (node->i_size - bytes_copied) < BLOCK_SIZE ? 
                                         (node->i_size - bytes_copied) : BLOCK_SIZE;
                memcpy((uint8_t*)buffer + bytes_copied, buf.buf, bytes_to_copy);
                bytes_copied += bytes_to_copy;
                blocks_read++;
            }
        }
    }
}
// → Reads all blocks of an inode (direct, indirect, double indirect) into buffer
/* =================== FILESYSTEM INITIALIZATION (Multiple Structures) =================== */

void create_ext2(void){
    // 1. Write filesystem signature to block 0
    write_blocks((void*)fs_signature, 0, 1);
    
    // 2. Initialize superblock
    memset(&superblock, 0, sizeof(superblock));
    superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
    superblock.s_blocks_count = DISK_SPACE / BLOCK_SIZE;
    superblock.s_r_blocks_count = 0;
    
    // Calculate metadata blocks per group: 1 block_bitmap + 1 inode_bitmap + 16 inode_table
    uint32_t metadata_blocks_per_group = 1 + 1 + INODES_TABLE_BLOCK_COUNT; // = 18
    uint32_t total_metadata_blocks = 3 + (GROUPS_COUNT * metadata_blocks_per_group); // 3 = boot + super + bgd
    
    superblock.s_free_blocks_count = superblock.s_blocks_count - total_metadata_blocks - 1; // -1 for root dir
    superblock.s_free_inodes_count = (INODES_PER_GROUP * GROUPS_COUNT) - 1; // -1 for root
    superblock.s_first_data_block = 0;
    superblock.s_first_ino = 2;
    superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
    superblock.s_frags_per_group = BLOCKS_PER_GROUP;
    superblock.s_inodes_per_group = INODES_PER_GROUP;
    superblock.s_magic = EXT2_SUPER_MAGIC;
    superblock.s_prealloc_blocks = 0;
    superblock.s_prealloc_dir_blocks = 0;
    
    write_blocks(&superblock, 1, 1);
    
    // 3. Create and write block group descriptor table to block 2
    memset(&bgd_table, 0, sizeof(bgd_table));
    
    uint32_t current_block = 3; // Start after boot(0), superblock(1), bgd_table(2)
    
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        bgd_table.table[i].bg_block_bitmap = current_block++;
        bgd_table.table[i].bg_inode_bitmap = current_block++;
        bgd_table.table[i].bg_inode_table = current_block;
        current_block += INODES_TABLE_BLOCK_COUNT; // Skip 16 blocks for inode table
        
        bgd_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - metadata_blocks_per_group;
        bgd_table.table[i].bg_free_inodes_count = INODES_PER_GROUP;
        bgd_table.table[i].bg_used_dirs_count = 0;
    }
    
    write_blocks(&bgd_table, 2, 1);
    
    // 4. Initialize bitmaps for each block group
    for (uint32_t group = 0; group < GROUPS_COUNT; group++) {
        uint8_t block_bitmap[BLOCK_SIZE] = {0};
        
        if (group == 0) {
            // Mark metadata blocks as used in group 0
            // Blocks 0-2: boot, superblock, bgd_table
            // Blocks 3-20: group 0 metadata (bitmap + bitmap + 16 inode table)
            // Total: blocks 0-20 = 21 blocks
            
            // Mark blocks 0-7 (first byte = 0xFF)
            block_bitmap[0] = 0xFF;
            
            // Mark blocks 8-15 (second byte = 0xFF)
            block_bitmap[1] = 0xFF;
            
            // Mark blocks 16-20 (third byte = 0x1F = 00011111)
            block_bitmap[2] = 0x1F;
        }
        
        write_blocks(block_bitmap, bgd_table.table[group].bg_block_bitmap, 1);
        
        // Initialize inode bitmap (all free)
        uint8_t inode_bitmap[BLOCK_SIZE] = {0};
        write_blocks(inode_bitmap, bgd_table.table[group].bg_inode_bitmap, 1);
        
        // Initialize inode table (all zeros)
        uint8_t zero_block[BLOCK_SIZE] = {0};
        for (uint32_t j = 0; j < INODES_TABLE_BLOCK_COUNT; j++) {
            write_blocks(zero_block, bgd_table.table[group].bg_inode_table + j, 1);
        }
    }
    
    // 5. Allocate root inode and create root directory
    uint32_t root_inode_num = allocate_node();
    struct EXT2Inode root_inode = {0};
    init_directory_table(&root_inode, root_inode_num, root_inode_num);
    
    // Update bgd for root directory
    bgd_table.table[0].bg_used_dirs_count = 1;
    write_blocks(&bgd_table, 2, 1);
}     
// → Creates EXT2Superblock, EXT2BlockGroupDescriptorTable, root directory

void initialize_filesystem_ext2(void) {
    if (is_empty_storage()) {
        create_ext2();
    } else {
        struct BlockBuffer buf;
        read_blocks(&buf, 1, 1);
        memcpy(&superblock, buf.buf, sizeof(superblock));
        
        read_blocks(&buf, 2, 1);
        memcpy(&bgd_table, buf.buf, sizeof(bgd_table));
    }
}
// → Entry point: calls is_empty_storage() then create_ext2() if needed

/* =================== HIGH-LEVEL OPERATIONS (EXT2DriverRequest) =================== */

int8_t read_directory(struct EXT2DriverRequest *prequest) {
    uint32_t parent_group = inode_to_bgd(prequest->parent_inode);
    uint32_t parent_local = inode_to_local(prequest->parent_inode);
    if (parent_group >= GROUPS_COUNT) return 3;

    struct EXT2Inode parent_node;
    struct BlockBuffer buf;
    uint32_t parent_inode_block = bgd_table.table[parent_group].bg_inode_table + (parent_local / INODES_PER_TABLE);
    uint32_t parent_inode_offset = (parent_local % INODES_PER_TABLE) * INODE_SIZE;
    
    read_blocks(&buf, parent_inode_block, 1);
    memcpy(&parent_node, buf.buf + parent_inode_offset, sizeof(parent_node));
    
    if (!(parent_node.i_mode & EXT2_S_IFDIR)) return 3; // Parent is not a directory
    
    // Search across all directory blocks (direct, indirect, double indirect)
    uint32_t dir_blocks = (parent_node.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int found = 0;
    struct EXT2DirectoryEntry *found_entry = NULL;
    uint32_t blocks_searched = 0;
    
    // Search direct blocks (0-11)
    for (uint32_t block_idx = 0; block_idx < 12 && blocks_searched < dir_blocks && !found; block_idx++) {
        if (parent_node.i_block[block_idx] == 0) continue;
        
        uint8_t dir_block[BLOCK_SIZE];
        read_blocks(dir_block, parent_node.i_block[block_idx], 1);
        
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
        uint32_t offset = 0;
        
        while (offset < BLOCK_SIZE && entry->inode != 0) {
            char *name = get_entry_name(entry);
            if (entry->name_len == prequest->name_len &&
                memcmp(name, prequest->name, entry->name_len) == 0) {
                found = 1;
                found_entry = entry;
                break;
            }
            offset += entry->rec_len;
            entry = get_next_directory_entry(entry);
        }
        blocks_searched++;
    }
    
    // Search single indirect blocks if needed
    if (!found && blocks_searched < dir_blocks && parent_node.i_block[12] != 0) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, parent_node.i_block[12], 1);
        
        for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; j++) {
            if (indirect_blocks[j] == 0) continue;
            
            uint8_t dir_block[BLOCK_SIZE];
            read_blocks(dir_block, indirect_blocks[j], 1);
            
            struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
            uint32_t offset = 0;
            
            while (offset < BLOCK_SIZE && entry->inode != 0) {
                char *name = get_entry_name(entry);
                if (entry->name_len == prequest->name_len &&
                    memcmp(name, prequest->name, entry->name_len) == 0) {
                    found = 1;
                    found_entry = entry;
                    break;
                }
                offset += entry->rec_len;
                entry = get_next_directory_entry(entry);
            }
            blocks_searched++;
        }
    }
    
    // Search double indirect blocks if needed
    if (!found && blocks_searched < dir_blocks && parent_node.i_block[13] != 0) {
        uint32_t double_indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(double_indirect_blocks, parent_node.i_block[13], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; i++) {
            if (double_indirect_blocks[i] == 0) continue;
            
            uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(indirect_blocks, double_indirect_blocks[i], 1);
            
            for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; j++) {
                if (indirect_blocks[j] == 0) continue;
                
                uint8_t dir_block[BLOCK_SIZE];
                read_blocks(dir_block, indirect_blocks[j], 1);
                
                struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
                uint32_t offset = 0;
                
                while (offset < BLOCK_SIZE && entry->inode != 0) {
                    char *name = get_entry_name(entry);
                    if (entry->name_len == prequest->name_len &&
                        memcmp(name, prequest->name, entry->name_len) == 0) {
                        found = 1;
                        found_entry = entry;
                        break;
                    }
                    offset += entry->rec_len;
                    entry = get_next_directory_entry(entry);
                }
                blocks_searched++;
            }
        }
    }
    
    if (!found) return 1;
    
    uint32_t target_inode_num = found_entry->inode;
    uint32_t target_group = inode_to_bgd(target_inode_num);
    uint32_t target_local = inode_to_local(target_inode_num);
    
    if (target_group >= GROUPS_COUNT) return 1;
    
    struct EXT2Inode target_node;
    uint32_t target_inode_block = bgd_table.table[target_group].bg_inode_table + (target_local / INODES_PER_TABLE);
    uint32_t target_inode_offset = (target_local % INODES_PER_TABLE) * INODE_SIZE;
    
    read_blocks(&buf, target_inode_block, 1);
    memcpy(&target_node, buf.buf + target_inode_offset, sizeof(target_node));
    
    if (!(target_node.i_mode & EXT2_S_IFDIR)) return 2; 
    
    // Read all directory blocks into prequest->buf
    read_node_blocks(&target_node, prequest->buf);
    
    return 0;
}

int8_t read(struct EXT2DriverRequest request){ 
    // 1. Load block group descriptor table
    struct EXT2BlockGroupDescriptorTable *bgdt = &bgd_table;

    // 2. Locate parent inode
    uint32_t parent_group = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    if (parent_group >= GROUPS_COUNT) {
        return 4; // Parent folder invalid
    }
    uint32_t parent_table_block = bgdt->table[parent_group].bg_inode_table + (parent_local / INODES_PER_TABLE);
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, parent_table_block, 1);
    struct EXT2Inode *parent_inode = &inode_table.table[parent_local % INODES_PER_TABLE];
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 4; // Parent folder invalid
    }
    if (parent_inode->i_block[0] == 0) return -1;

    // 3. Search across all directory blocks for the requested name (direct, indirect, double indirect)
    uint32_t dir_blocks = (parent_inode->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int found = 0;
    struct EXT2DirectoryEntry *found_entry = NULL;
    uint32_t blocks_searched = 0;
    
    // Search direct blocks (0-11)
    for (uint32_t block_idx = 0; block_idx < 12 && blocks_searched < dir_blocks && !found; block_idx++) {
        if (parent_inode->i_block[block_idx] == 0) continue;
        
        uint8_t dir_block[BLOCK_SIZE];
        read_blocks(dir_block, parent_inode->i_block[block_idx], 1);

        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
        uint32_t offset = 0;
        
        while (offset < BLOCK_SIZE && entry->inode != 0) {
            char *name = get_entry_name(entry);
            if (entry->name_len == request.name_len &&
                memcmp(name, request.name, entry->name_len) == 0) {
                found = 1;
                found_entry = entry;
                break;
            }
            offset += entry->rec_len;
            entry = get_next_directory_entry(entry);
        }
        blocks_searched++;
    }
    
    // Search single indirect blocks if needed
    if (!found && blocks_searched < dir_blocks && parent_inode->i_block[12] != 0) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, parent_inode->i_block[12], 1);
        
        for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; j++) {
            if (indirect_blocks[j] == 0) continue;
            
            uint8_t dir_block[BLOCK_SIZE];
            read_blocks(dir_block, indirect_blocks[j], 1);

            struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
            uint32_t offset = 0;
            
            while (offset < BLOCK_SIZE && entry->inode != 0) {
                char *name = get_entry_name(entry);
                if (entry->name_len == request.name_len &&
                    memcmp(name, request.name, entry->name_len) == 0) {
                    found = 1;
                    found_entry = entry;
                    break;
                }
                offset += entry->rec_len;
                entry = get_next_directory_entry(entry);
            }
            blocks_searched++;
        }
    }
    
    // Search double indirect blocks if needed
    if (!found && blocks_searched < dir_blocks && parent_inode->i_block[13] != 0) {
        uint32_t double_indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(double_indirect_blocks, parent_inode->i_block[13], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; i++) {
            if (double_indirect_blocks[i] == 0) continue;
            
            uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(indirect_blocks, double_indirect_blocks[i], 1);
            
            for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_searched < dir_blocks && !found; j++) {
                if (indirect_blocks[j] == 0) continue;
                
                uint8_t dir_block[BLOCK_SIZE];
                read_blocks(dir_block, indirect_blocks[j], 1);

                struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
                uint32_t offset = 0;
                
                while (offset < BLOCK_SIZE && entry->inode != 0) {
                    char *name = get_entry_name(entry);
                    if (entry->name_len == request.name_len &&
                        memcmp(name, request.name, entry->name_len) == 0) {
                        found = 1;
                        found_entry = entry;
                        break;
                    }
                    offset += entry->rec_len;
                    entry = get_next_directory_entry(entry);
                }
                blocks_searched++;
            }
        }
    }
    if (!found) {
        return 3; // Not found
    }

    // 5. Load the file inode
    uint32_t file_inode_num = found_entry->inode;
    uint32_t file_group = inode_to_bgd(file_inode_num);
    uint32_t file_local = inode_to_local(file_inode_num);
    if (file_group >= GROUPS_COUNT) {
        return 3; // Not found
    }
    uint32_t file_table_block = bgdt->table[file_group].bg_inode_table + (file_local / INODES_PER_TABLE);
    read_blocks(&inode_table, file_table_block, 1);
    struct EXT2Inode *file_inode = &inode_table.table[file_local % INODES_PER_TABLE];

    // Check if it's a regular file
    if (!(file_inode->i_mode & EXT2_S_IFREG)) {
        return 1; // Not a file
    }

    // 6. Read file data: direct and indirect blocks
    uint32_t file_size = file_inode->i_size;
    if (request.buffer_size < file_size) {
        return 2; // Not enough buffer
    }

    // Read all file blocks into request.buf
    read_node_blocks(file_inode, request.buf);
    return 0;
}  
// → Reads file data using EXT2Inode.i_block[] array

int8_t write(struct EXT2DriverRequest *request){
    // 1. Load parent inode
    uint32_t parent_group = inode_to_bgd(request->parent_inode);
    uint32_t parent_local = inode_to_local(request->parent_inode);
    
    if (parent_group >= GROUPS_COUNT) {
        return 2; // Invalid parent folder
    }
    
    uint32_t parent_table_block = bgd_table.table[parent_group].bg_inode_table + (parent_local / INODES_PER_TABLE);
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, parent_table_block, 1);
    struct EXT2Inode *parent_inode = &inode_table.table[parent_local % INODES_PER_TABLE];
    
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 2; // Invalid parent folder
    }
    
    // 2. Search across all directory blocks for existing entry and find insertion point
    uint32_t dir_blocks = (parent_inode->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    struct EXT2DirectoryEntry *insertion_entry = NULL;
    uint32_t insertion_block_idx = 0;
    
    for (uint32_t block_idx = 0; block_idx < dir_blocks && block_idx < 12; block_idx++) {
        if (parent_inode->i_block[block_idx] == 0) continue;
        
        uint8_t dir_block[BLOCK_SIZE];
        read_blocks(dir_block, parent_inode->i_block[block_idx], 1);
        
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
        uint32_t offset = 0;
        
        while (offset < BLOCK_SIZE && entry->inode != 0) {
            char *name = get_entry_name(entry);
            // Check if file/folder already exists
            if (entry->name_len == request->name_len && memcmp(name, request->name, entry->name_len) == 0) {
                return 1; // File/folder already exists
            }
            
            offset += entry->rec_len;
            entry = get_next_directory_entry(entry);
        }
        
        // Found empty entry for insertion
        if (entry != NULL && entry->inode == 0) {
            insertion_entry = entry;
            insertion_block_idx = block_idx;
            break;
        }
    }
    
    // If no insertion point found, use the last block
    if (insertion_entry == NULL) {
        insertion_block_idx = 0; // Use first block for simplicity
        uint8_t dir_block[BLOCK_SIZE];
        read_blocks(dir_block, parent_inode->i_block[0], 1);
        
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
        uint32_t offset = 0;
        
        while (offset < BLOCK_SIZE && entry->inode != 0) {
            offset += entry->rec_len;
            entry = get_next_directory_entry(entry);
        }
        insertion_entry = entry;
    }
    
    // 3. Check if we have space to add new entry, allocate new block if needed
    uint16_t new_entry_len = get_entry_record_len(request->name_len);
    
    if (insertion_entry == NULL || insertion_entry->rec_len < new_entry_len) {
        // Need to allocate a new directory block
        uint32_t current_dir_blocks = (parent_inode->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        // Expand directory by one block using allocate_node_blocks
        parent_inode->i_size += BLOCK_SIZE;
        
        // Create empty directory block data
        uint8_t new_dir_block[BLOCK_SIZE] = {0};
        struct EXT2DirectoryEntry *empty_entry = get_directory_entry(new_dir_block, 0);
        empty_entry->inode = 0;
        empty_entry->rec_len = BLOCK_SIZE;
        
        // Use allocate_node_blocks to properly handle direct/indirect/double indirect allocation
        allocate_node_blocks(new_dir_block, parent_inode, inode_to_bgd(request->parent_inode));
        
        // Sync the updated parent inode
        sync_node(parent_inode, request->parent_inode);
        
        // Use the new block for insertion (it's the last block allocated)
        insertion_entry = empty_entry;
        insertion_block_idx = current_dir_blocks;
    }
    
    // 4. Allocate new inode for the entry
    uint32_t new_inode_num = allocate_node();
    
    struct EXT2Inode new_inode = {0};
    
    // 5. Get the actual block number for writing
    uint32_t actual_block_num = 0;
    if (insertion_block_idx < 12) {
        actual_block_num = parent_inode->i_block[insertion_block_idx];
    } else if (insertion_block_idx < 12 + BLOCK_SIZE / sizeof(uint32_t)) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, parent_inode->i_block[12], 1);
        actual_block_num = indirect_blocks[insertion_block_idx - 12];
    } else {
        uint32_t double_indirect_index = (insertion_block_idx - 12 - BLOCK_SIZE / sizeof(uint32_t)) / (BLOCK_SIZE / sizeof(uint32_t));
        uint32_t indirect_index = (insertion_block_idx - 12 - BLOCK_SIZE / sizeof(uint32_t)) % (BLOCK_SIZE / sizeof(uint32_t));
        
        uint32_t double_indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(double_indirect_blocks, parent_inode->i_block[13], 1);
        
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, double_indirect_blocks[double_indirect_index], 1);
        actual_block_num = indirect_blocks[indirect_index];
    }
    
    // Re-read and write the directory block where we made changes
    uint8_t dir_block[BLOCK_SIZE];
    read_blocks(dir_block, actual_block_num, 1);
    
    // Find the insertion point in the actual dir_block
    struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
    uint32_t offset = 0;
    
    while (offset < BLOCK_SIZE && entry->inode != 0) {
        offset += entry->rec_len;
        entry = get_next_directory_entry(entry);
    }
    
    // Now entry points to the empty entry in the actual dir_block
    uint16_t available_space = entry->rec_len;
    
    // Create new entry in the actual directory block
    entry->inode = new_inode_num;
    entry->name_len = request->name_len;
    entry->rec_len = get_entry_record_len(request->name_len);
    memcpy(get_entry_name(entry), request->name, request->name_len);
    
    // Update the next entry's rec_len to fill remaining space
    struct EXT2DirectoryEntry *next_entry = get_next_directory_entry(entry);
    next_entry->inode = 0;
    next_entry->rec_len = available_space - entry->rec_len;
    
    // 6. Handle directory vs file creation
    if (request->is_directory) {
        // Create directory
        entry->file_type = EXT2_FT_DIR;
        
        // Write updated parent directory block first
        write_blocks(dir_block, actual_block_num, 1);
        
        // Initialize directory table (will allocate blocks internally)
        init_directory_table(&new_inode, new_inode_num, request->parent_inode);
        
        // Update bgd used_dirs_count
        uint32_t new_bgd = inode_to_bgd(new_inode_num);
        bgd_table.table[new_bgd].bg_used_dirs_count++;
        write_blocks(&bgd_table, 2, 1);
        
    } else {
        // Create file
        entry->file_type = EXT2_FT_REG_FILE;
        
        // Write updated parent directory block first
        write_blocks(dir_block, actual_block_num, 1);
        
        // Setup file inode
        new_inode.i_size = request->buffer_size;
        new_inode.i_mode = EXT2_S_IFREG;
        
        // Allocate blocks and write data (will set i_blocks internally)
        if (request->buffer_size > 0) {
            allocate_node_blocks(request->buf, &new_inode, inode_to_bgd(new_inode_num));
        }
        
        // Save file inode to disk
        sync_node(&new_inode, new_inode_num);
    }
    
    return 0;
}
// → Creates new file or updates existing file
int8_t delete(struct EXT2DriverRequest request) {
    // 1. Load parent inode
    uint32_t parent_group = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    
    if (parent_group >= GROUPS_COUNT) {
        return 3; // Parent folder invalid
    }
    
    uint32_t parent_table_block = bgd_table.table[parent_group].bg_inode_table + 
                                   (parent_local / INODES_PER_TABLE);
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, parent_table_block, 1);
    struct EXT2Inode *parent_inode = &inode_table.table[parent_local % INODES_PER_TABLE];
    
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 3; // Parent folder invalid
    }
    
    // 2. Read parent directory block
    uint8_t dir_block[BLOCK_SIZE];
    read_blocks(dir_block, parent_inode->i_block[0], 1);
    
    // 3. Search for target entry
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
    char *name = get_entry_name(entry);
    
    while (offset < BLOCK_SIZE && entry->inode != 0) {
        if (entry->name_len == request.name_len &&
            memcmp(name, request.name, entry->name_len) == 0) {
            break; // Found target
        }
        offset += entry->rec_len;
        entry = get_next_directory_entry(entry);
        name = get_entry_name(entry);
    }
    
    if (entry->inode == 0) {
        return 1; // Not found
    }
    
    // 4. Load target inode
    uint32_t target_inode_num = entry->inode;
    uint32_t target_group = inode_to_bgd(target_inode_num);
    uint32_t target_local = inode_to_local(target_inode_num);
    
    if (target_group >= GROUPS_COUNT) {
        return 1; // Not found
    }
    
    uint32_t target_table_block = bgd_table.table[target_group].bg_inode_table + 
                                   (target_local / INODES_PER_TABLE);
    read_blocks(&inode_table, target_table_block, 1);
    struct EXT2Inode *target_inode = &inode_table.table[target_local % INODES_PER_TABLE];
    
    // 5. If directory, check if empty
    if ((target_inode->i_mode & EXT2_S_IFDIR)) {
        if (!is_directory_empty(target_inode_num)) {
            return 2; 
        }
        
        // Update bgd used_dirs_count
        bgd_table.table[target_group].bg_used_dirs_count--;
        write_blocks(&bgd_table, 2, 1);
    }
    
    // 6. Remove directory entry and compact directory block
    uint32_t entry_offset = (uint8_t*)entry - dir_block;
    uint32_t entry_size = entry->rec_len;
    uint32_t next_offset = entry_offset + entry_size;
    uint32_t remaining = BLOCK_SIZE - next_offset;
    if (remaining > 0) {
        // Shift all subsequent entries left to fill the gap
        memmove(dir_block + entry_offset, dir_block + next_offset, remaining);
    }
    // Mark the last entry's rec_len to fill the rest of the block
    struct EXT2DirectoryEntry *last_entry = get_directory_entry(dir_block, 0);
    uint32_t last_offset = 0;
    while (last_entry->inode!=0 && last_offset<BLOCK_SIZE) {
        last_offset += last_entry->rec_len;
        last_entry = get_next_directory_entry(last_entry);
    }
    last_entry->rec_len = BLOCK_SIZE - last_offset;

    // Write updated directory block
    write_blocks(dir_block, parent_inode->i_block[0], 1);

    // 7. Deallocate inode and its blocks
    deallocate_node(target_inode_num);

    return 0; // Success
}
// → Removes file/directory and cleans up all resources

int8_t rename_entry(struct EXT2DriverRequest *request) {
    if (request == NULL || request->name == NULL || request->buf == NULL) {
        return -1;
    }

    uint32_t parent_inode_num = request->parent_inode;
    uint32_t parent_group = inode_to_bgd(parent_inode_num);
    uint32_t parent_local = inode_to_local(parent_inode_num);

    if (parent_group >= GROUPS_COUNT) {
        return 3;
    }

    uint32_t parent_table_block = bgd_table.table[parent_group].bg_inode_table +
                                  (parent_local / INODES_PER_TABLE);
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, parent_table_block, 1);
    struct EXT2Inode *parent_inode = &inode_table.table[parent_local % INODES_PER_TABLE];

    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 3;
    }

    uint8_t dir_block[BLOCK_SIZE];
    read_blocks(dir_block, parent_inode->i_block[0], 1);

    struct EXT2DirectoryEntry *entry = get_directory_entry(dir_block, 0);
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *target = NULL;
    struct EXT2DirectoryEntry *cursor = entry;

    while (offset < BLOCK_SIZE && cursor->inode != 0) {
        char *entry_name = get_entry_name(cursor);
        if (cursor->name_len == request->name_len &&
            memcmp(entry_name, request->name, cursor->name_len) == 0) {
            target = cursor;
        }
        offset += cursor->rec_len;
        cursor = get_next_directory_entry(cursor);
    }

    if (target == NULL) {
        return 1;
    }

    size_t new_name_len = request->buffer_size;
    if (new_name_len == 0) {
        new_name_len = strlen((char *) request->buf);
    }

    if (new_name_len == 0 || new_name_len > 255) {
        return -1;
    }

    cursor = entry;
    offset = 0;
    while (offset < BLOCK_SIZE && cursor->inode != 0) {
        if (cursor != target) {
            char *entry_name = get_entry_name(cursor);
            if (cursor->name_len == new_name_len &&
                memcmp(entry_name, request->buf, new_name_len) == 0) {
                return 2;
            }
        }
        offset += cursor->rec_len;
        cursor = get_next_directory_entry(cursor);
    }

    size_t header_size = sizeof(struct EXT2DirectoryEntry);
    if (target->rec_len < header_size + new_name_len) {
        return 4;
    }

    char *target_name = get_entry_name(target);
    memset(target_name, 0, target->rec_len - header_size);
    memcpy(target_name, request->buf, new_name_len);
    target->name_len = (uint16_t) new_name_len;

    write_blocks(dir_block, parent_inode->i_block[0], 1);
    return 0;
}


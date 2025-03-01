/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    uint8_t i, eidx;
    uint8_t total_entries;
    if(buffer->full) total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    else if(buffer->in_offs > buffer->out_offs) total_entries = buffer->in_offs - buffer->out_offs;
    else total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs + buffer->in_offs;
    for (i = 0; i < total_entries; i++) {
        eidx = (buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if (char_offset < buffer->entry[eidx].size) {
            *entry_offset_byte_rtn = char_offset;
            return &buffer->entry[eidx];
        } else {
            char_offset -= buffer->entry[eidx].size;
        }
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    buffer->entry[buffer->in_offs] = *add_entry;
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;    
    if (buffer->full) {
        buffer->out_offs = buffer->in_offs;
    } else {
        if (buffer->in_offs == buffer->out_offs)
        {
            buffer->full = true;
        }
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

/**
* Return the calculated total offset given an entry index and offset.
* Function will return -1 if index + offset was out of range.
*/
long aesd_circulr_buffer_get_offset(struct aesd_circular_buffer *buffer, uint8_t idx, size_t off)
{
    uint8_t i, eidx;
    uint8_t total_entries;
    long    global_offset = 0;
    if(buffer->full) total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    else if(buffer->in_offs > buffer->out_offs) total_entries = buffer->in_offs - buffer->out_offs;
    else total_entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs + buffer->in_offs;

    if (idx >= total_entries) return -1;

    for (i = 0; i < idx; i++) {
        eidx = (buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        global_offset += buffer->entry[eidx].size;
    }
    eidx = (buffer->out_offs + idx) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (off > buffer->entry[eidx].size) return -1;

    return global_offset + off;
}
/**
 * @file dynarray.h
 * @author OldSchoolPixels.com
 * @brief A dynamic array implementation
 * @version 0.1
 * @date 2025-02-07
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef OSP_DYNARRAY_H
#define OSP_DYNARRAY_H

#include <stddef.h>
#include <stdint.h>

typedef struct _osp_dynarray *osp_dynarray_t;

extern osp_dynarray_t osp_dynarray_new(const size_t element_size, const size_t initial_capacity, const size_t increment);
extern void osp_dynarray_push(osp_dynarray_t array, const void *data);
extern void osp_dynarray_pop(osp_dynarray_t array, void *data);
extern void osp_dynarray_enqueue(osp_dynarray_t array, const void *data);
extern void osp_dynarray_int_dequeue(osp_dynarray_t array, void *data);
extern void osp_dynarray_get(osp_dynarray_t array, size_t idx, void *data);
extern void osp_dynarray_remove_at(osp_dynarray_t array, size_t idx, void *data);
extern void osp_dynarray_clear(osp_dynarray_t array);
extern void osp_dynarray_delete(osp_dynarray_t array);
extern void *osp_dynarray_fwd_iter_start(osp_dynarray_t array);
extern void *osp_dynarray_bck_iter_start(osp_dynarray_t array);
extern uint8_t osp_dynarray_iter_check(osp_dynarray_t array);
extern void osp_dynarray_fwd_iter_next(osp_dynarray_t array, void **iter);
extern void osp_dynarray_bck_iter_next(osp_dynarray_t array, void **iter);
extern size_t osp_dynarray_get_count(osp_dynarray_t array);
extern void *osp_dynarray_get_data(osp_dynarray_t array);

#define osp_dynarray_add osp_dynarray_enqueue

#endif
#include "dynarray.h"
#include <stdlib.h>
#include <string.h>

struct _osp_dynarray
{
    void *data;
    size_t element_size;
    size_t capacity;
    size_t increment;
    size_t count;
    size_t iterations;
};

void osp_dynarray_grow(osp_dynarray_t array)
{
    if(array == NULL)
        return;

    void *new_array = calloc(array->capacity + array->increment, array->element_size);
    memcpy(new_array, array->data, array->capacity * array->element_size);
    free(array->data);
    array->data = new_array;
    array->capacity += array->increment;
}

osp_dynarray_t osp_dynarray_new(const size_t element_size, const size_t initial_capacity, const size_t increment)
{
    if(element_size <= 0 || initial_capacity <= 0)
        return NULL;

    size_t real_increment = increment;
    if(real_increment <= 0)
        real_increment = 1;

    osp_dynarray_t array = (osp_dynarray_t)calloc(1, sizeof(struct _osp_dynarray));
    array->element_size = element_size;
    array->capacity = initial_capacity;
    array->increment = real_increment;
    array->data = calloc(array->capacity, array->element_size);
    array->count = 0;

    return array;
}

void osp_dynarray_push(osp_dynarray_t array, const void *data)
{
    if(array == NULL)
        return;

    if(array->count >= array->capacity)
        osp_dynarray_grow(array);

    memmove(array->data + array->element_size, array->data, array->count * array->element_size);
    memcpy(array->data, data, array->element_size);
    array->count++;
}

void osp_dynarray_pop(osp_dynarray_t array, void *data)
{
    if(data == NULL || array == NULL || array->count == 0)
        return;

    memcpy(data, array->data, array->element_size);
    if(array->count > 1)
        memmove(array->data, array->data + array->element_size, (array->count - 1) * array->element_size);
    array->count--;
}

void osp_dynarray_enqueue(osp_dynarray_t array, const void *data)
{
    if(array == NULL || data == NULL)
        return;

    if(array->count >= array->capacity)
        osp_dynarray_grow(array);

    memcpy(array->data + (array->count * array->element_size), data, array->element_size);
    array->count++;
}

void osp_dynarray_int_dequeue(osp_dynarray_t array, void *data)
{
    if(data == NULL || array == NULL || array->count == 0)
        return;

    array->count--;
    memcpy(data, array->data + (array->count * array->element_size), array->element_size);
}

void osp_dynarray_get(osp_dynarray_t array, size_t idx, void *data)
{
    if(data == NULL || array == NULL || idx >= array->count)
        return;

    memcpy(data, array->data + (idx * array->element_size), array->element_size);
}

void osp_dynarray_remove_at(osp_dynarray_t array, size_t idx, void *data)
{
    if(array == NULL || idx >= array->count)
        return;

    void *dest_ptr = array->data + (idx * array->element_size);
    if(data != NULL)
        memcpy(data, dest_ptr, array->element_size);

    if(idx < array->count - 1)
    {
        void *src_ptr = dest_ptr + array->element_size;
        memmove(dest_ptr, src_ptr, (array->count - idx - 1) * array->element_size);
    }

    array->count--;
}

void osp_dynarray_clear(osp_dynarray_t array)
{
    if(array == NULL)
        return;

    array->count = 0;
}

void osp_dynarray_delete(osp_dynarray_t array)
{
    if(array == NULL)
        return;

    if(array->data != NULL)
        free(array->data);

    array->capacity = 0;
    array->count = 0;
    array->element_size = 0;
    array->increment = 0;

    free(array);
}

void *osp_dynarray_fwd_iter_start(osp_dynarray_t array)
{
    if(array == NULL)
        return NULL;

    array->iterations = array->count;
    return array->data;
}

void *osp_dynarray_bck_iter_start(osp_dynarray_t array)
{
    if(array == NULL)
        return NULL;

    array->iterations = array->count;
    return array->data + (array->count * array->element_size);
}

uint8_t osp_dynarray_iter_check(osp_dynarray_t array)
{
    return array->iterations > 0;
}

void osp_dynarray_fwd_iter_next(osp_dynarray_t array, void **iter)
{
    array->iterations--;
    (*iter) += array->element_size;
}

void osp_dynarray_bck_iter_next(osp_dynarray_t array, void **iter)
{
    array->iterations--;
    (*iter) -= array->element_size;
}

size_t osp_dynarray_get_count(osp_dynarray_t array)
{
    if(array == NULL)
        return 0;

    return array->count;
}

void *osp_dynarray_get_data(osp_dynarray_t array)
{
    if(array == NULL)
        return NULL;

    return array->data;
}
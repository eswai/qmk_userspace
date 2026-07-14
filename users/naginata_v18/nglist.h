#pragma once
#include QMK_KEYBOARD_H

#define LIST_SIZE 5 // 1同時押しの最大キー数(NGListの最大サイズ)

typedef struct {
    uint16_t elements[LIST_SIZE];
    int size;
} NGList;

void initializeList(NGList *);
bool addToList(NGList *, uint16_t);
int includeList(NGList *, uint16_t);
void copyList(NGList *, NGList *);

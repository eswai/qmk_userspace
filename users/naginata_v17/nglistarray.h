#pragma once
#include QMK_KEYBOARD_H
#include "nglist.h"

// 整理: LIST_SIZE(nglist.hで定義。1同時押しの最大キー数)とは別概念の
//       「入力バッファの深さ」なので、名前を分けて重複定義を解消する
#define LIST_ARRAY_SIZE 5 // 入力バッファ(NGListArray)の最大サイズ

typedef struct {
    NGList elements[LIST_ARRAY_SIZE];
    int size;
} NGListArray;

void initializeListArray(NGListArray *);
bool addToListArray(NGListArray *, NGList *);
bool removeFromListArrayAt(NGListArray *, int);

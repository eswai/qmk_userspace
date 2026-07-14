#include "nglist.h"

// 集合を初期化する関数
void initializeList(NGList *list) {
    list->size = 0;
}

// 要素を集合に追加する関数
bool addToList(NGList *list, uint16_t element) {
    if (list->size >= LIST_SIZE) {
        return false;
    }

    // 集合に要素を追加
    list->elements[list->size++] = element;
    return true;
}

int includeList(NGList *list, uint16_t element) {
    // 要素のインデックスを見つける
    for (int i = 0; i < list->size; i++) {
        if (list->elements[i] == element) {
            return i;
            break;
        }
    }

    return -1;
}

void copyList(NGList *a, NGList *b) {
    initializeList(b);
    for (int i = 0; i < a->size; i++) {
        addToList(b, a->elements[i]);
    }
}

#include "mosaicSource/common/common.h"

dictNode* create_node(char *key, char *value) {
    dictNode *new_node = malloc(sizeof(dictNode));
    if (new_node == NULL) {
        return NULL;
    }
    new_node->key = strdup(key);
    new_node->value = strdup(value);
    new_node->next = NULL;
    return new_node;
}

void insert_node(dictNode **head, char *key, char *value) {
    dictNode *new_node = create_node(key, value);
    if (*head == NULL) {
        *head = new_node;
    } else {
        dictNode *current_node = *head;
        while (current_node->next != NULL) {
            current_node = current_node->next;
        }
        current_node->next = new_node;
    }
}

void free_dictionary(dictNode *head) {
    dictNode *current_node = head;
    while (current_node != NULL) {
        dictNode *temp = current_node;
        current_node = current_node->next;
        free(temp->key);
        free(temp->value);
        free(temp);
    }
}

char* get_value(dictNode *head, char *key) {
    dictNode *current_node = head;
    while (current_node != NULL) {
        if (strcmp(current_node->key, key) == 0) {
            return current_node->value;
        }
        current_node = current_node->next;
    }
    return NULL;
}

void printDictionary(dictNode *head) {
    dictNode *current;
    fprintf(stderr,"Dictionary Contents\n");
    for(current = head; current != NULL; current=current->next) {
        fprintf(stderr,"\tKey %s  Value %s\n", current->key, current->value);
    }

}
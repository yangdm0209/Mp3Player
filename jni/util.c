#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

util_dict_t *util_dict_new(void) {
	return (util_dict_t *) calloc(1, sizeof(util_dict_t));
}

void util_dict_free(util_dict_t *dict) {
	util_dict_t *next;

	while (dict) {
		next = dict->next;

		if (dict->key)
			free(dict->key);
		if (dict->val)
			free(dict->val);
		free(dict);

		dict = next;
	}
}

const char *util_dict_get(util_dict_t *dict, const char *key) {
	if (!key)
		return NULL;
	while (dict) {
		if (dict->key && !strcmp(key, dict->key))
			return dict->val;
		dict = dict->next;
	}
	return NULL;
}

int util_dict_set(util_dict_t *dict, const char *key, const char *val) {
	util_dict_t *prev;

	if (!dict || !key) {
		return 0;
	}

	prev = NULL;
	while (dict) {
		if (!dict->key || !strcmp(dict->key, key))
			break;
		prev = dict;
		dict = dict->next;
	}

	if (!dict) {
		dict = util_dict_new();
		if (!dict) {
			return 0;
		}
		if (prev)
			prev->next = dict;
	}

	if (dict->key)
		free(dict->val);
	else if (!(dict->key = strdup(key))) {
		if (prev)
			prev->next = NULL;
		util_dict_free(dict);

		return 0;
	}

	dict->val = strdup(val);
	if (!dict->val) {
		return 0;
	}

	return 1;
}
void util_dict_print(util_dict_t *dict)
{
    util_dict_t *next;

    while (dict) {
        next = dict->next;
        if (dict->key && dict->val)
            LOGI("util item is: %s = %s", dict->key ,dict->val);
        dict = next;
    }
}

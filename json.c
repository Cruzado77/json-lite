#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <time.h>

char *jsonTeste = 
    "{"
        "\"id\": 1,"
        "\"submit\": \"2012-04-23T18:25:43.511Z\","
        "\"nascimento\": \"2010-04-23\","
        "\"nome\": \"Raphael\","
        "\"endereco\": {"
            "\"numero\": \"4242\","
            "\"coord\": {"
                "\"lat\": -11.46765,"
                "\"lon\": -35.4 "
            "}"
        "},"
        "\"cond\": [\"asds\",\"2\",\"3\"]"
    "}";

#define TYPE_OBJECT     0
#define TYPE_ARRAY      2
#define TYPE_NUMBER     3
#define TYPE_STRING     4

typedef struct sJsonItem
{
    struct sJsonItem* parent;
    struct sJsonItem* next;
    char* key;
    void *_obj;
    int _typeIdentifier;
    int _index;
    int _length;
} JsonItem;

JsonItem* JsonParse(char* string);

void JsonClear(JsonItem* root);

const JsonItem* JsonGetObject(const JsonItem* json, const char* key);

const JsonItem* JsonGetItem(const JsonItem* json, const char* key);

const double* JsonGetNumber(const JsonItem* json, const char* key);

double JsonGetNumberOrDefault(const JsonItem* json, const char* key);

const JsonItem* JsonGetArray(const  JsonItem* json, const char* key);

const char* JsonGetString(const JsonItem* json, const char* key);

struct tm* JsonGetDateTime(const JsonItem* json, const char* key);

/*
Gera string array no heap e precisa de free;
*/
char** JsonGetStringArray(const JsonItem* json, const char* key, int* arrayLength);

double** JsonGetNumberArray(const JsonItem* json, const char* key, int* arrayLength);

struct tm * getTime()
{
    // Obtém o tempo atual
    time_t tempoInicio;
    time(&tempoInicio);

    // Converte o tempo atual em uma estrutura tm para obter informações detalhadas
    return localtime(&tempoInicio);
}

int main()
{
    struct tm* tempoInicio = getTime();

    JsonItem* root = JsonParse(jsonTeste);

    const JsonItem* search = JsonGetObject(root, "endereco:numero");
    
    const char* numero = JsonGetString(root, "endereco:numero");

    search = JsonGetArray(root, "cond");

    int length;
    double** cond = JsonGetNumberArray(root, "cond", &length);

    for(int i = 0; i < length; i++)
    {
        printf("cond[%d] = %d\n", i,(int) *cond[i]);
    }

    free(cond);

    const JsonItem* coord = JsonGetObject(root, "endereco:coord");

    printf("coord: {lat: %f, lon: %f}\n", 
        JsonGetNumberOrDefault(coord, "lat"),
        JsonGetNumberOrDefault(coord, "lon"));

    struct tm* submit = JsonGetDateTime(root, "submit");
    printf("submit: %s\n", asctime(submit));
    free(submit);
    submit = JsonGetDateTime(root, "nascimento");
    printf("nascimento: %s\n", asctime(submit));
    free(submit);

    JsonClear(root);

    struct tm* tempoFim = getTime();

    int tempo = tempoFim->tm_sec - tempoInicio->tm_sec;
    return 0;
}

static inline int modulo(int i)
{
    return i >= 0 ? i : -1;
}

static char* getString(char** cPointer, char startCaracter)
{
    size_t len = strlen(*cPointer);
    if(!len)
    {
        return NULL;
    }

    char key[len + 1], *c;
    int i;
    for (i = 0; **cPointer != startCaracter; (*cPointer)++, i++)
    {
        c = *cPointer;

        if(*c == '\\')
        {
            if(*(c + 1) != '"')
            {
                return NULL;
            }
            else
            {
                key[i] = '"';
                
                (*cPointer)++;

                continue;
            }
        }
        key[i] = **cPointer;
    }
    key[i] = '\0';

    char* ret = malloc(strlen(key) + 1);

    strcpy(ret, key);

    return ret;
}

double* getNumber(char** cPointer)
{
    char delimiter = ',';
    size_t len = strlen(*cPointer);
    if(!len)
    {
        return NULL;
    }

    char key[len + 1], *c;
    int i;
    for (i = 0; isdigit(**cPointer) || **cPointer == '.'; (*cPointer)++, i++)
    {
        key[i] = **cPointer;
    }
    key[i] = '\0';

    double* result = malloc(sizeof(double));

    char *point = NULL;
    if(point = strstr(key, "."))
    {
        *point = localeconv()->decimal_point[0];
    }

    *result = strtod(key, NULL);

    return result;
}

static JsonItem* create(JsonItem* model, JsonItem* before)
{
    JsonItem* new = malloc(sizeof(JsonItem));

    memcpy(new, model, sizeof(JsonItem));

    if(before)
    {
        before->next = new;
    }
    return new;
}

static JsonItem* createObj(JsonItem* parent, JsonItem* before, char* key)
{
    JsonItem def = {
        parent: parent,
        next: NULL,
        key: key,
        _obj: NULL,
        _typeIdentifier: TYPE_OBJECT,
        _index: parent ? parent->_length : 0,
        _length: 0
    };

    if(parent) parent->_length++;

    return create(&def, before);
}

static JsonItem* createNumber(JsonItem* parent, JsonItem* before, char* key, double* number)
{
    JsonItem def = {
        parent: parent,
        next: NULL,
        key: key,
        _obj: number,
        _typeIdentifier: TYPE_NUMBER,
        _index: parent ? parent->_length : 0,
        _length: 0
    };
    
    if(parent) parent->_length++;

    return create(&def, before);
}

static JsonItem* createString(JsonItem* parent, JsonItem* before, char* key, char* obj)
{
    JsonItem def = {
        parent: parent,
        next: NULL,
        key: key,
        _obj: obj,
        _typeIdentifier: TYPE_STRING,
        _index: parent ? parent->_length : 0,
        _length: 0
    };

    if(parent) parent->_length++;

    return create(&def, before);
}

static JsonItem* createArray(JsonItem* parent, JsonItem* before, char* key)
{
    JsonItem def = {
        parent: parent,
        next: NULL,
        key: key,
        _obj: NULL,
        _typeIdentifier: TYPE_ARRAY,
        _index: parent ? parent->_length : 0,
        _length: 0
    };

    if(parent) parent->_length++;

    return create(&def, before);
}

JsonItem* JsonParse(char* string)
{
    char* c = string;
    bool pendingValue = true, inArray = false;
    int index = -1;
    char* key = NULL;

    JsonItem *root = NULL, *actual = NULL, *parent = NULL;

    while (*c)
    {
        if(*c == ' ')
        {
            goto next;
        }
        if(*c == '{')
        {
            actual = createObj(parent, actual, key);

            parent = actual;

            pendingValue = false;
        }
        if(*c == '"' || *c == '\'')
        {
            char startCaracter = *c;

            *c++;

            if(!pendingValue)
            {
                key = getString(&c, startCaracter);
                
                pendingValue = true;
            }
            else
            {
                char *value = getString(&c, startCaracter);

                actual = createString(parent, actual, key, value);
                
                pendingValue = false;
            }
        }
        if(pendingValue && isdigit(*c))
        {
            double* number = getNumber(&c);

            actual = createNumber(parent, actual, key, number);
            
            pendingValue = false;
        }
        if(pendingValue && *c == '[')
        {
            actual = createArray(parent, actual, key);

            parent = actual;
        }
        if(*c == ']' || *c == '}')
        {
            parent = actual->parent ? actual->parent->parent : NULL;
        }
next:
        if(!parent) parent = actual;
        
        if(parent->_typeIdentifier == TYPE_ARRAY)
        {
            key = NULL;
            pendingValue = true;
        }

        if(!root && actual) root = actual;
        *c++;
    }
    
    return root;
}

void JsonClear(JsonItem* root)
{
    if(!root)
    {
        return;
    }

    free(root->_obj);
    free(root->key);

    JsonClear(root->next);

    free(root);
}

static const JsonItem* searchItem(const JsonItem* json, const char* strictkey)
{
    if(!json || !strictkey) return NULL;

    if(isdigit(strictkey[0]))
    {
        long index = strtol(strictkey, NULL, 10);

        if(index == json->_index) return json;
    }
    else if(json->key && !strcmp(strictkey, json->key)) return json;

    return searchItem(json->next, strictkey);
}

const JsonItem* JsonGetItem(const JsonItem* json, const char* key)
{
    if(!json || !key) return NULL;

    char keycpy[strlen(key) + 1];

    strcpy(keycpy, key);

    //todo adicionar controle de profundidade
    char* subkey = strtok(keycpy, ":");

    const JsonItem* search = json;

    while (subkey && search)
    {
        search = searchItem(search, subkey);

        subkey = strtok(NULL, ":");
    }
    
    return search;
}

const JsonItem* JsonGetObject(const JsonItem* json, const char* key)
{
    const JsonItem* item = JsonGetItem(json, key);

    return item && item->_typeIdentifier == TYPE_OBJECT ? item : NULL;
}

const double* JsonGetNumber(const JsonItem* json, const char* key)
{
    const JsonItem* item = JsonGetItem(json, key);

    return item && item->_typeIdentifier == TYPE_NUMBER ? (double *) item->_obj : NULL;
}

const JsonItem* JsonGetArray(const  JsonItem* json, const char* key)
{
    const JsonItem* item = JsonGetItem(json, key);
    
    return item && item->_typeIdentifier == TYPE_ARRAY ? item : NULL;
}

const char* JsonGetString(const JsonItem* json, const char* key)
{
    const JsonItem* item = JsonGetItem(json, key);

    return item && item->_typeIdentifier == TYPE_STRING ? (char *) item->_obj : NULL;
}

double JsonGetNumberOrDefault(const JsonItem* json, const char* key)
{
    const double* value = JsonGetNumber(json, key);

    return value ? *value : 0;
}

double** JsonGetNumberArray(const JsonItem* json, const char* key, int* arrayLength)
{
    const JsonItem *array = JsonGetArray(json, key);
    if(!array)
    {
        if(arrayLength) *arrayLength = 0;

        return NULL;
    }

    JsonItem* element = array->next;
    
    size_t size = sizeof(double*) * array->_length;
    double** res = malloc(size);
    memset(res, '\0', size);

    int index = 0;
    while (element && element->parent == array)
    {
        if(element->_typeIdentifier == TYPE_NUMBER)
        {
            res[index] = element->_obj;
            index++;
        }

        element = element->next;
    }

    if(arrayLength) *arrayLength = index;
    
    return res;
}

char** JsonGetStringArray(const JsonItem* json, const char* key, int* arrayLength)
{
    const JsonItem *array = JsonGetArray(json, key);
    if(!array)
    {
        if(arrayLength) *arrayLength = 0;

        return NULL;
    }

    JsonItem* element = array->next;
    
    size_t size = sizeof(char*) * array->_length;
    char** res = malloc(size);
    memset(res, '\0', size);

    int index = 0;
    while (element->parent == array)
    {
        if(element->_typeIdentifier == TYPE_STRING)
        {
            res[index] = element->_obj;
            index++;
        }
    }

    if(arrayLength) *arrayLength = index;
    
    return res;
}


struct tm* JsonGetDateTime(const JsonItem* json, const char* key)
{
    const char* str = JsonGetString(json, key);

    if(!str) return NULL;

    struct tm* time = malloc(sizeof(struct tm));
    memset(time, 0, sizeof(struct tm));

    double sec;
    if (sscanf(str, "%d-%d-%dT%d:%d:%lfZ", &time->tm_year, &time->tm_mon, &time->tm_mday, &time->tm_hour, &time->tm_min, &sec) == 6)
    {
        time->tm_sec = sec;
        goto defined_time;
    }

    if (sscanf(str, "%d-%d-%d", &time->tm_year, &time->tm_mon, &time->tm_mday) == 3)
    {
        goto defined_time;
    }

    free(time);

    return NULL;

defined_time:
    time->tm_year -= 1900;
    mktime(time);
    return time;
}

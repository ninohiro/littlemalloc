#include<stdint.h>
#include<stddef.h>

#define ALIGN 16

struct header_t;

struct header_t {
    struct header_t *next, *prev;
    struct header_t *free_next, *free_prev;
} __attribute__((aligned(ALIGN)));
typedef struct header_t header;

static __attribute__((aligned(ALIGN))) uint8_t mem[256*256*16];

static header *last_p;
static header *last_free_p;
static void *end_p=(void*)mem;

static header *free_p[16];

static inline void memzero(void *p, size_t n){
    for(size_t i=0;i<n;i++){
        *(uint8_t*)(p+i)=0;
    }
}

static header *expand_last(size_t n){
    if(end_p + n > (void*)mem + sizeof(mem)){
        return NULL;
    }
    header *h=end_p;
    end_p += n;
    memzero((void*)h, sizeof(header));
    h->prev=last_p;
    if(last_p){
        last_p->next=h;
    }
    last_p=h;
    return h;
}
static void shrink_last(){
    header *h=last_p;
    last_p=last_p->prev;
    end_p = (void*)h;
}

static header *split(header *h, size_t n){
    header *h2=(void*)h + n;
    memzero((void*)h2, sizeof(header));
    h2->next=h->next;
    if(h->next){
        h->next->prev=h2;
    }
    h2->prev=h;
    h->next=h2;
    if(last_p == h){
        last_p = h2;
    }
    return h2;
}

static void erase_free(header *h){
    for(int j=0;j<16;j++){
        if(free_p[j]==h){
            free_p[j]=h->free_next;
        }
    }
    if(h->free_next){
        h->free_next->free_prev=h->free_prev;
    }else{
        last_free_p=h->free_prev;
    }
    if(h->free_prev){
        h->free_prev->free_next=h->free_next;
    }
    h->free_next=NULL;
    h->free_prev=NULL;
}

static inline int get_rank(size_t n){
    n>>=4;
    if(n==0){
        return -1;
    }
    int i;
    for(i=0;n>>(i+1);i++);
    return i;
}
static inline size_t get_space(header *h){
    return (!h->next ? end_p : (void*)h->next)-(void*)h;
}

static void insert_free(header *h){
    int rank=get_rank(get_space(h)-sizeof(header));
    h->free_next=free_p[rank];
    h->free_prev=!h->free_next ? last_free_p : h->free_next->free_prev;
    if(h->free_next){
        h->free_next->free_prev=h;
    }else{
        last_free_p=h;
    }
    if(h->free_prev){
        h->free_prev->free_next=h;
    }
    for(int j=0;j<=rank;j++){
        if(free_p[j]==h->free_next){
            free_p[j]=h;
        }
    }
}

void *littlemalloc(size_t size){
    if(size==0 || size>=256*256*16){
        return NULL;
    }
    size=(size + ALIGN - 1) & ~(ALIGN - 1);
    header *h=free_p[get_rank(size-1) + 1];
    size_t n=sizeof(header) + size;

    if(h==NULL){
        h=expand_last(n);
        if(!h){
            return NULL;
        }
        return (void*)h + sizeof(header);
    }

    size_t space=get_space(h);
    if(space-n > sizeof(header)){
        header *h2=split(h, n);
        erase_free(h);
        insert_free(h2);
        return (void*)h + sizeof(header);
    }else{
        erase_free(h);
        return (void*)h + sizeof(header);
    }
}

static inline int is_free(header *h){
    return h && (h->free_next || h==last_free_p);
}

static void expand_next(header *h){
    if(last_p == h->next){
        last_p = h;
    }
    header *h_next=h->next->next;
    h->next=h_next;
    if(h_next){
        h_next->prev=h;
    }
}

void littlefree(void *p){
    header *h=p-sizeof(header);
    if(is_free(h->next)){
        erase_free(h->next);
        expand_next(h);
    }
    if(is_free(h->prev)){
        erase_free(h->prev);
        h=h->prev;
        expand_next(h);
    }
    if(last_p==h){
        shrink_last();
    }else{
        insert_free(h);
    }
}

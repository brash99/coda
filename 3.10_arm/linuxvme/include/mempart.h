/*
   header file for the memory allocation system 
*/

#ifndef __MEM_PART__
#define __MEM_PART__

typedef struct danode			      /* Node of a linked list. (must be contained on an 8 byte boundary) */
{
  struct danode         *n;	              /* Points at the next node in the list. */
  struct danode         *p;	              /* Points at the previous node in the list. */
  struct rol_mem_part   *part;	              /* Which partition "owns" this node. */    
  int                   fd;		      /* File descriptor associated with this node. */
  char                  *current;	      /* Current data mark */
  unsigned int          left;	              /* data left to process (bytes) */
  unsigned int          type;                 /* data type - Holds Sync Event Info for CODA 3 */
  unsigned int          source;               /* data source */
  int                   dummy;                /* keep danode structure on an 8 byte memory boundary */
  void                  (*reader)();          /* routine to read data if data segment is empty */
  int                   nevent;               /* event number */
  unsigned int          length;	              /* Length of data to follow (bytes). */
  unsigned int          data[1];	      /* Node data. */
} DANODE;

typedef struct alist			      /* Header for a linked list. */
{
  DANODE        *f;		              /* head */
  DANODE        *l;		              /* tail */
  int            c;			      /* Number of nodes in list */
  int            to;
  void          (*add_cmd)(struct alist *li);     /* command to call on list add */
  void          *clientData;               /* data to pass for add_cmd */ 
} DALIST;

typedef struct rol_mem_part *ROL_MEM_ID;

typedef struct rol_mem_part{
    DANODE	 node;		/* global partition list */
    DALIST	 list;		/* free item list */
    char	 name[40];	/* part name */
    void         (*free_cmd)(); /* command to call when fifo is empty */
    void         *clientData;   /* data to pass for free_cmd */ 
    int		 size;		/* size of a single item */
    int		 incr;		/* Flag incr=1 when memory pool is fragmented */
    int		 total;		/* total items allocated so far */

    long          part[1];	/* pointer to memory pool (defined as 4 bytes for 32bit systems and 8 bytes for 64 bit systems) */
} ROL_MEM_PART;

#define listInit(li) {bzero((char *) (li), sizeof(DALIST));}

#define listGet(li,no) {\
  (no) = 0;\
  if ((li)->c){\
    (li)->c--;\
    (no) = (li)->f; \
    (li)->f = (li)->f->n;\
  };\
  if (!(li)->c) {\
    (li)->l = 0;\
  }\
}

#define listWait(li,no) {(no) = 0;(li)->to = 0; \
                         while(((li)->c == 0) && ((li)->to == 0)) { \
                                             }; \
                         if ((li)->to == 0) { \
                                              (li)->c--; \
                                              (no) = (li)->f; \
                                              (li)->f = (li)->f->n; \
                                              if (!(li)->c) \
                                                (li)->l = 0; \
                                              } else { \
                                                (no) = (void *) -1; \
                                              }; \
                        }
 
/* call add_cmd (if it exists) whenever a buffer is added to a list */
#define listAdd(li,no) {if(! (li)->c ){(li)->f = (li)->l = (no);(no)->p = 0;} else \
			  {(no)->p = (li)->l;(li)->l->n = (no);(li)->l = (no);} (no)->n = 0;(li)->c++;\
		          if((li)->add_cmd != NULL) (*((li)->add_cmd)) ((li));  }

#define listSnip(li,no) {if ((no)->p) {(no)->p->n =(no)->n;} else {(li)->f = (no)->n;} \
if ((no)->n) {(no)->n->p =(no)->p;} else {(li)->l = (no)->p;} \
(li)->c--;if ((li)->c==0) (li)->f = (li)->l = (DANODE *)0;(no)->p=(no)->n= (DANODE *)0;}

#define listCount(li) ((li)->c)
#define listFirst(li) ((li)->f)
#define listLast(li) ((li)->l)
#define listNext(no) ((no)->n)

/* call free_cmd (if it exists) when the pool is full of buffers */

#define partFreeItem(pItem) { \
    if ((pItem)->part == 0) { \
        free(pItem); pItem = 0; \
    } else { \
        listAdd (&pItem->part->list, pItem);\
    } \
    if(pItem->part->free_cmd != NULL) \
      (*(pItem->part->free_cmd)) (pItem->part->clientData); \
}

#define partEmpty(pPart) (pPart->list.c == 0)

#define partGetItem(p,i) {listGet(&(p->list),i);}

extern void libPartInit();
extern void partFree(ROL_MEM_ID pPart);
extern void partFreeAll();  
extern int  partIncr ( ROL_MEM_ID pPart, int c);
extern ROL_MEM_ID partCreate (char *name, int size, int c, int incr);
extern void partReInit (ROL_MEM_ID pPart);
extern int  partReInitAll();
#endif







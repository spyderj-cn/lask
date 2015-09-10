
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"

typedef unsigned int ngx_uint_t;
typedef int ngx_int_t;
#define ngx_thread_volatile 
#define ngx_inline
typedef unsigned char u_char;

/******************************************************************************
	nginx rbtree
******************************************************************************/

/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


typedef ngx_uint_t  ngx_rbtree_key_t;
typedef ngx_int_t   ngx_rbtree_key_int_t;


typedef struct ngx_rbtree_node_s  ngx_rbtree_node_t;

struct ngx_rbtree_node_s {
    ngx_rbtree_key_t       key;
    ngx_rbtree_node_t     *left;
    ngx_rbtree_node_t     *right;
    ngx_rbtree_node_t     *parent;
    u_char                 color;
    u_char                 data;
};


typedef struct ngx_rbtree_s  ngx_rbtree_t;

typedef void (*ngx_rbtree_insert_pt) (ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

struct ngx_rbtree_s {
    ngx_rbtree_node_t     *root;
    ngx_rbtree_node_t     *sentinel;
    ngx_rbtree_insert_pt   insert;
};


#define ngx_rbtree_init(tree, s, i)                                           \
    ngx_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i


#define ngx_rbt_red(node)               ((node)->color = 1)
#define ngx_rbt_black(node)             ((node)->color = 0)
#define ngx_rbt_is_red(node)            ((node)->color)
#define ngx_rbt_is_black(node)          (!ngx_rbt_is_red(node))
#define ngx_rbt_copy_color(n1, n2)      (n1->color = n2->color)


/* a sentinel must be black */

#define ngx_rbtree_sentinel_init(node)  ngx_rbt_black(node)


static ngx_inline ngx_rbtree_node_t *
ngx_rbtree_min(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}

static ngx_inline ngx_rbtree_node_t *
ngx_rbtree_max(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
	while (node->right != sentinel) {
		node = node->right;
	}
	
	return node;
}

/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */


static ngx_inline void ngx_rbtree_left_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);
static ngx_inline void ngx_rbtree_right_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);


void
ngx_rbtree_insert(ngx_thread_volatile ngx_rbtree_t *tree,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = (ngx_rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        ngx_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && ngx_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    ngx_rbtree_left_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            temp = node->parent->parent->left;

            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    ngx_rbtree_right_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    ngx_rbt_black(*root);
}


void
ngx_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


void
ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */

        p = ((ngx_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


void
ngx_rbtree_delete(ngx_thread_volatile ngx_rbtree_t *tree,
    ngx_rbtree_node_t *node)
{
    ngx_uint_t           red;
    ngx_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = (ngx_rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = ngx_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        ngx_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = ngx_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        ngx_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && ngx_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->right)) {
                    ngx_rbt_black(w->left);
                    ngx_rbt_red(w);
                    ngx_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->right);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->left)) {
                    ngx_rbt_black(w->right);
                    ngx_rbt_red(w);
                    ngx_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->left);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    ngx_rbt_black(temp);
}


static ngx_inline void
ngx_rbtree_left_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static ngx_inline void
ngx_rbtree_right_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


/******************************************************************************
	lua interface
******************************************************************************/
#define BLOCK_BUFSIZ			1024 			

typedef struct _slot {
	struct _slot *next;
}slot_t;

typedef struct _block {
	struct _block *next;
	char buf[BLOCK_BUFSIZ - sizeof(void*)];
}block_t;

typedef struct _pool {
	size_t csize;
	block_t *blocks;
	slot_t *slots;
}pool_t;

#define 	LAYER_NULL_INITIALIZER		{0, NULL, NULL}

static void init_blocks(block_t *block, size_t csize)
{
	size_t num = sizeof(block->buf) / csize;
	char *p = block->buf;
	slot_t *slot = (slot_t*)p, *next;
	
	for (size_t i = 0; i < (num - 1); i++) {
		next = (slot_t*)(p + csize);
		slot->next = next;
		slot = next;
		p += csize;
	}
	slot->next = NULL;
}

static void pool_init(pool_t *pool, size_t csize)
{
	if (csize < sizeof(slot_t))
		csize = sizeof(slot_t);
	csize = (csize + 3) & ~3;

	pool->csize = csize;
	pool->blocks = NULL;
	pool->slots = NULL;
}

static inline void* pool_alloc(pool_t *pool)
{
	slot_t *slot = pool->slots;
	if (slot == NULL) {
		block_t *block = (block_t*)MALLOC(sizeof(block_t));
		block->next = pool->blocks;
		pool->blocks = block;
		init_blocks(block, pool->csize);
		slot = pool->slots = (slot_t*)block->buf;
	}
	pool->slots = slot->next;
	return slot;
}

static inline void pool_free(pool_t *pool, void *p)
{
	slot_t *slot = (slot_t*)p;
	slot->next = pool->slots;
	pool->slots = slot;
}

static inline void pool_fini(pool_t *pool)
{
	block_t *block = pool->blocks, *next;
	while (block != NULL) {
		next = block->next;
		FREE(block);
		block = next;
	}
	pool->blocks = NULL;
	pool->slots = NULL;
	pool->csize = 0;
}


#define RBTREE_META			"meta(rbtree)"

typedef struct rbtree_node rbtree_node_t;
struct rbtree_node{
	ngx_rbtree_node_t ngx;
	int value;
};

typedef struct {
	ngx_rbtree_t ngx;
	size_t num;
	ngx_rbtree_node_t sentinel;
	pool_t pool;
}rbtree_t;

/*
** tree = rbtree.new()
*/
static int lrbtree_new(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)lua_newuserdata(L, sizeof(rbtree_t));
	
	ngx_rbtree_init(&rbtree->ngx, &rbtree->sentinel, ngx_rbtree_insert_value);
	pool_init(&rbtree->pool, sizeof(rbtree_node_t));
	rbtree->num = 0;

	l_setmetatable(L, -1, RBTREE_META);
	
	return 1;
}

/*
** rbtree:insert(key, value)
** 
** both key and value are integers
*/
static int lrbtree_insert(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	ngx_rbtree_key_t key;
	int value;
	rbtree_node_t *node;
	
	key = (ngx_rbtree_key_t)luaL_checkinteger(L, 2);
	value = luaL_checkinteger(L, 3);
	
	node = (rbtree_node_t*)pool_alloc(&rbtree->pool);
	node->ngx.key = key;
	node->value = value;
	ngx_rbtree_insert(&rbtree->ngx, &node->ngx);
	
	rbtree->num++;
	
	return 0;
}

static rbtree_node_t* rbtree_find(rbtree_t *rbtree, ngx_rbtree_key_t key)
{
	ngx_rbtree_node_t* node = rbtree->ngx.root;
	
	while (node != rbtree->ngx.sentinel) {
		int diff = (int)(node->key - key);
		if (diff == 0)
			break;
		else if (diff > 0)
			node = node->left;
		else
			node = node->right;
	}
	
	return (node != rbtree->ngx.sentinel ? (rbtree_node_t*)node : NULL);	
}

/*
** value/nil = rbtree:find(key)
*/
static int lrbtree_find(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	rbtree_node_t *node;
	
	node = rbtree_find(rbtree, (ngx_rbtree_key_t)luaL_checkinteger(L, 2));
	if (node != NULL) {
		lua_pushinteger(L, node->value);
	} else {
		lua_pushnil(L);
	}
	
	return 1;
}

static rbtree_node_t* rbtree_find_byboth(rbtree_node_t *node, ngx_rbtree_node_t *sentinel, 
							ngx_rbtree_key_t key, int value)
{
	rbtree_node_t *p;

	if (node->ngx.key == key && node->value == value)
		return node;
	
	/* if multiple nodes have the same key, we have to search the whole (or half of) the branch */
	
	if (node->ngx.key <= key && node->ngx.right != sentinel) {
		p = rbtree_find_byboth((rbtree_node_t*)node->ngx.right, sentinel, key, value);
		if (p != NULL)
			return p;
	}

	if (node->ngx.key >= key && node->ngx.left != sentinel) {
		p = rbtree_find_byboth((rbtree_node_t*)node->ngx.left, sentinel, key, value);
		if (p != NULL)
			return p;
	}
	
	return NULL;
}

/*
** value/nil = rbtree:del(key[, value])
**
** return the value of the deleted node, or nil if the node does not exist
*/
static int lrbtree_del(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	rbtree_node_t *node;
	
	node = rbtree_find(rbtree, (ngx_rbtree_key_t)luaL_checkinteger(L, 2));
	if (node != NULL && lua_gettop(L) > 2) {
		node = rbtree_find_byboth(node, rbtree->ngx.sentinel, node->ngx.key, luaL_checkinteger(L, 3));
	}
	
	if (node != NULL) {
		lua_pushinteger(L, node->value);
		
		ngx_rbtree_delete(&rbtree->ngx, &node->ngx);
		pool_free(&rbtree->pool, node);
		rbtree->num--;
	} else {
		lua_pushnil(L);
	}
	
	return 1;
}

static int lrbtree_minmax_impl(lua_State *L, bool is_min)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);

	if (rbtree->ngx.root != rbtree->ngx.sentinel) {	
		ngx_rbtree_node_t *node;

		if (is_min) 
			node = ngx_rbtree_min(rbtree->ngx.root, rbtree->ngx.sentinel);
		else
			node = ngx_rbtree_max(rbtree->ngx.root, rbtree->ngx.sentinel);
			
		lua_pushinteger(L, node->key);
		lua_pushinteger(L, ((rbtree_node_t*)node)->value);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

static int lrbtree_delminmax_impl(lua_State *L, bool is_min)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	ngx_rbtree_node_t *node;
	ngx_rbtree_key_t ref;
	
	if (rbtree->ngx.root == rbtree->ngx.sentinel)
		return 0;

	ref = (ngx_rbtree_key_t)luaL_checkinteger(L, 2);
	if (is_min)
		node = ngx_rbtree_min(rbtree->ngx.root, rbtree->ngx.sentinel);
	else
		node = ngx_rbtree_max(rbtree->ngx.root, rbtree->ngx.sentinel);
	
	if (node != rbtree->ngx.sentinel 
		&& ((is_min && node->key <= ref) || (!is_min && node->key >= ref))) {
		lua_pushinteger(L, node->key);
		lua_pushinteger(L, ((rbtree_node_t*)node)->value);
		
		ngx_rbtree_delete(&rbtree->ngx, node);
		pool_free(&rbtree->pool, node);
		rbtree->num--;
		
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/*
** key/nil, value/nil = rbtree:min()
*/
static int lrbtree_min(lua_State *L)
{
	return lrbtree_minmax_impl(L, true);
}

/*
** key/nil, value/nil = rbtree:delmin(ref)
** 
** get the key-value pair of the minimum node if its key <= ref, otherwise (nil, nil)
*/
static int lrbtree_delmin(lua_State *L)
{
	return lrbtree_delminmax_impl(L, true);
}

/*
** key/nil, value/nil = rbtree:max()
*/
static int lrbtree_max(lua_State *L)
{
	return lrbtree_minmax_impl(L, false);
}

/*
** key/nil, value/nil = rbtree:delmax(ref)
** 
** get the key-value pair of the minimum node if its key >= ref, otherwise (nil, nil)
*/
static int lrbtree_delmax(lua_State *L)
{
	return lrbtree_delminmax_impl(L, false);
}

/*
** rbtree:deckey(offset)
*/
static int lrbtree_deckey(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	ngx_rbtree_key_t offset;
	ngx_rbtree_node_t *stk[2 * 32], *node, *sentinel;
	int idx = 0;

	offset = (ngx_rbtree_key_t)luaL_checkinteger(L, 2);
	sentinel = rbtree->ngx.sentinel;
	
	node = rbtree->ngx.root;
	if (node != sentinel) 
		stk[idx++] = node;
		
	while (idx > 0) {
		node = stk[--idx];
		
		if (node->key >= offset)
			node->key -= offset;
		else
			node->key = 0;
		
		if (node->left != sentinel)
			stk[idx++] = node->left;
		
		if (node->right != sentinel)
			stk[idx++] = node->right;
	}
	
	return 0;
}

static void rbtree_dump(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel, int indent)
{		
	if (node != sentinel) {
		char tabs[16] = {0};
		
		for (int i = 0; i < indent; i++) 
			tabs[i] = '\t';
			
		printf(tabs);
		printf("(%d, %d)\n", node->key, ((rbtree_node_t*)node)->value);
		
		rbtree_dump(node->left, sentinel, indent + 1);
		rbtree_dump(node->right, sentinel, indent + 1);
	}
}

/*
**	
*/
static int lrbtree_dump(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	rbtree_dump(rbtree->ngx.root, rbtree->ngx.sentinel, 0);
	return 0;
}

/*
** rbtree:__gc
*/
static int lrbtree_gc(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	pool_fini(&rbtree->pool);
	return 0;
}

/*
** rbtree:__tostring
*/
static int lrbtree_tostring(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	char buf[64];
	
	snprintf(buf, sizeof(buf), "rbtree (0x%08x, %d nodes)", (unsigned int)rbtree, (int)rbtree->num);
	lua_pushstring(L, buf);
	
	return 1;
}

/*
** rbtree:__len
*/
static int lrbtree_len(lua_State *L)
{
	rbtree_t *rbtree = (rbtree_t*)luaL_checkudata(L, 1, RBTREE_META);
	lua_pushinteger(L, (int)rbtree->num);
	return 1;
}


static const luaL_Reg meta_methods[] = {
	{"__gc", lrbtree_gc},
	{"__tostring", lrbtree_tostring},
	{"__len", lrbtree_len},
	{NULL, NULL}
};

static const luaL_Reg methods[] = {
	{"new", lrbtree_new},
	{"insert", lrbtree_insert},
	{"find", lrbtree_find},
	{"del", lrbtree_del},
	{"min", lrbtree_min},
	{"delmin", lrbtree_delmin},
	{"max", lrbtree_max},
	{"delmax", lrbtree_delmax},
	{"deckey", lrbtree_deckey},
	{"dump", lrbtree_dump},
	{NULL, NULL},
};

int l_openrbtree(lua_State *L)
{
	luaL_register(L, "rbtree", methods);
	l_register_metatable(L, RBTREE_META, meta_methods);
	lua_pop(L, 1);
	return 0;
}

#include <crisp.h>

// This is a type code which will be filled in when imported
uint64_t MAP;

// from http://burtleburtle.net/bob/hash/integer.html
unsigned int hash32(unsigned int a) {
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

unsigned int hash64(uint64_t a){
    return hash32(a >> 32) ^ hash32(a);
}

unsigned int get_key_hash(cell k) {
    uint64_t k_val;
    if(TYPE(k) == S64) k_val = S64_VAL(k);
    else if(TYPE(k) == SYMBOL) k_val = SYM_STR(k);
    return hash64(k_val);
}

// Each node is shaped like:
// (keyhash, left, right, val1, val2, ...)
cell tree_insert(cell root, unsigned int keyhash, cell kvp) {
    if(!root) return cons(make_s64(keyhash), cons(NIL, cons(NIL, LIST1(kvp))));
    unsigned int node_keyhash = (unsigned int) S64_VAL(car(root));
    if(keyhash < node_keyhash) {
        ((pair*) PTR(cdr(root)))->car = tree_insert(cdar(root), keyhash, kvp);
    } else if(keyhash > node_keyhash) {
        ((pair*) PTR(cddr(root)))->car = tree_insert(cddar(root), keyhash, kvp);
    } else {
        ((pair*) PTR(cddr(root)))->cdr = cons(kvp, cdddr(root));
    }
    return root;
}

cell tree_lookup(cell root, unsigned int keyhash, cell key) {
    if(!root) return NIL;
    unsigned int node_keyhash = (unsigned int) S64_VAL(car(root));
    if(keyhash < node_keyhash) {
        return tree_lookup(cdar(root), keyhash, key);
    } else if(keyhash > node_keyhash) {
        return tree_lookup(cddar(root), keyhash, key);
    } else {
        return assoc(key, cdddr(root));
    }
}

cell map_lookup(cell args, cell env) {
    if (!args || TYPE(cdr(args)) != PAIR || TYPE(cdar(args) != MAP)) return NIL;
    return tree_lookup(cdar(args), get_key_hash(car(args)), car(args));
}

void print_tree(cell tree, int level) {
    for(int i = level; i > 0; i--){
        DPRINTF(" ", i);
    }
    if(!tree) {
        DPRINTF("---\n", 0);
        return;
    }
    DPRINTF("%u %s\n", S64_VAL(car(tree)), print_cell(cdddr(tree)));
    if(!cdar(tree) && !cddar(tree)) return;
    print_tree(cdar(tree), level + 2);
    print_tree(cddar(tree), level + 2);
}

cell mkmap(cell args, cell env) {
    if (!args || TYPE(car(args)) != PAIR) return NIL;
    args = car(args);
    cell root = NIL;
    while(IS_PAIR(args) && IS_PAIR(car(args))) {
        cell el = car(args);
        unsigned int kh = get_key_hash(car(el));
        root = tree_insert(root, kh, el);
        args = cdr(args);
    }
    print_tree(root, 0);
    return CAST(root, MAP);
}

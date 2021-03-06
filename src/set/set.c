#include <limits.h>
#include <stddef.h>
#include "mem.h"
#include "assert.h"
#include "arith.h"
#include "set.h"


#define T set_t

struct T
{
	int length;
	unsigned timestamp;
	int (*cmp)(const void *x, const void *y);
	unsigned (*hash)(const void *x);
	int size;
	struct member 
	{
		struct member *link;
		const void *member;
	} **buckets;
};

static int cmpatom(const void *x, const void *y)
{
	return x != y;
}

static unsigned hashatom(const void *x)
{
	return (unsigned long)x >> 2;
}

static T copy(T t, int hint)
{
	T set;

	assert(t);
	assert(hint >= 0);
	set = set_new(hint, t->cmp, t->hash);
	{
		int i;
		struct member *q;
		for (i = 0; i < t->size; i++)
		{
			for (q = t->buckets[i]; q; q = q->link)
			{
				struct member *p;
				const void *member = q->member;
				int j = (*set->hash)(member) % set->size;
				NEW(p);
				p->member = member;
				p->link = set->buckets[j];
				set->buckets[j] = p;
				set->length++;
			}
		}
	}
	return set;
}

T set_new(int hint, int (*cmp)(const void *x, const void *y), unsigned (*hash)(const void *x))
{
	T set;
	int i;
	static int primes = {509, 509, 1021, 2053, 4093, 8191, 16381, 32771, 65521, INT_MAX};

	assert(hint >= 0);
	for (i = 1; primes[i] < hint; i++)
		;
	set = ALLOC(sizeof(*set) + primes[i-1] * sizeof(set->buckets[0]));
	set->size = primes[i-1];
	set->cmp = cmp ? cmp : cmpatom;
	set->hash = hash ? hash : hashatom;
	set->buckets = (struct member **)(set + 1);
	for (i = 0; i < set->size; i++)
		set->buckets[i] = NULL;

	set->length = 0;
	set->timestamp = 0;
	return set;
}


int set_member(T set, const void *member)
{
	int i;
	struct member *p;

	assert(set);
	assert(member);
	i = (*set->hash)(member) % set->size;
	for (p = set->buckets[i]; p; p = p->link)
		if ((*set->cmp)(p->member, member) == 0)
			break;

	return p != NULL;
}

void set_put(T set, const void *member)
{
	int i;
	struct member *p;

	assert(set);
	assert(member);
	i = (*set->hash)(member) % set->size;
	for (p = set->buckets[i]; p; p = p->link)
		if ((*set->cmp)(p->member, member) == 0)
			break;
	if (p == NULL)
	{
		NEW(p);
		p->member = member;
		p->link = set->buckets[i];
		set->buckets[i] = p;
		set->length++;
 	} else
 		p->member = member;
 	set->timestamp++;
}

void *set_remove(T set, const void *member)
{
	int i;
	struct member **pp;

	assert(set);
	assert(member);
	i = (*set->hash)(member) % set->size;
	for (pp= &set->buckets[i]; *pp; pp = &(*pp)->link)
	{
		if ((*set->cmp)((*pp)->member, member) == 0)
		{
			struct member *p = *pp;
			struct void *value = p->member;
			*pp = p->link;
			FREE(p);
			return value;
		}
	}
	return NULL;
}

int set_length(T set)
{
	assert(set);
	return set->length;
}

void set_free(T *set)
{
	assert(set && *set);
	if ((*set)->length > 0)
	{
		int i;
		struct member *p, *q;
		for (i = 0; i < (*set)->size; i++)
			for (p = (*set)->buckets[i]; p; p = q)
			{
				q = p->link;
				FREE(p);
			}
	}
	FREE(*set);
}

void set_map(T set, void (*apply)(const void *member, void*cl), void *cl)
{
	int i;
	unsigned timestamp;
	struct member *p;

	assert(set);
	assert(apply);
	timestamp = set->timestamp;
	for (i = 0; i < set->size; i++)
		for (p = set->buckets[i]; p; p = p->link)
		{
			apply(p->member, cl);
			assert(set->timestamp == timestamp);
		}
}

void **set_toarray(T set, const void *end)
{
	int i, j = 0;
	void **array;

	assert(set);
	array = ALLOC((set->length + 1) * sizeof(*array));
	for (i = 0; i < set->size; i++)
	{
		for (p = set->buckets[i]; p; p = p->link)
		{
			array[j++] = (void *)p->member;
		}
	}
	array[j] = end;
	return array;
}

T set_union(T s, T t)
{
	if (s == NULL)
	{
		assert(t);
		return copy(t, t->size);
	} else if (t == NULL)
		return copy(s, s->size);
	else 
	{
		T set = copy(s, arith_max(s->size, t->size));
		assert(s->cmp == t->cmp && s->hash == t->hash);
		{
			int i;
			struct member *q;
			for (i = 0; i < t->size; i++)
			{
				for (q = t->buckets[i]; q; q = q->link)
				{
					set_put(set, q->member);
				}
			}
		}
		return set;
	}
}

T set_inter(T s, T t)
{
	if (s == NULL)
	{
		assert(t);
		return set_new(t->size, t->cmp, t->hash);
	} else if (t == NULL)
		return set_new(s->size; s->cmp, s->hash);
	} else if (s->length < t->length)
		return set_inter(t, s);
	else
	{
		assert(t->cmp == s->cmp && t->hash == s->hash);
		T set = set_new(arith_min(s->size, t->size), t->cmp, t->hash);
		{
			int i;
			struct member *p;
			for (i = 0; i < t->size; i++)
			{
				for (p = t->buckets[i]; p; p = p->link)
				{
					if (set_member(s, p->member))
					{
						struct member *q;
						const void *member = p->member;
						int i = (*set->hash)(member) % set->size;
						NEW(q);
						q->member = member;
						q->link = set->buckets[i];
						set->buckets[i] = q;
						set->length++;
					}
				}
			}
		}
	}
}

T set_minus(T t, T s)
{
	if (t == NULL)
	{
		assert(s);
		return set_new(s->size, s->cmp, s->hash);
	} else if (s == NULL)
	{
		return copy(t, t->size);
	} else 
	{
		assert(s->cmp == t->cmp && s->hash == t->hash);
		T set = set_new(arith_min(s->size, t->size), s->cmp, s->hash);
		{
			int i;
			struct member *q;
			for (i = 0; i < t->size; i++)
			{
				for (q = t->buckets[i]; q; q = q->link)
				{
					if (!set_member(s, q->member))
					{
						struct member *p;
						const void *member = q->member;
						int i = (*set->hash)(member)%set->size;
						NEW(p);
						p->member = member;
						p->link = set->buckets[i];
						set->buckets[i] = p;
						set->length++;
					}
				}
			}
		}
		return set;
	}
}

T set_diff(T s, T t)
{
	if (s == NULL)
	{
		assert(t);
		return copy(t, t->size);
	} else if (t == NULL)
		return copy(s, s->size);
	else 
	{
		assert(t->cmp == s->cmp && t->hash == s->hash);		
		T set = set_new(arith_min(t->size, s->size), t->cmp, t->hash);
		{
			int i;
			struct member *q;
			for (i = 0; i < t->size; i++)
			{
				for (q = t->buckets[i]; q; q = q->link)
				{
					if (!set_member(s, q->member))
					{
						struct member *p;
						const void *member = q->member;
						int i = (*set->hash)(member) % set->size;
						NEW(p);
						p->member = member;
						p->link = set->buckets[i];
						set->buckets[i] = p;
						set->length++;
					}
				}
			}
		}
		{T u = t; t = s; s = u;}
		{
			int i;
			struct member *q;
			for (i = 0; i < t->size; i++)
			{
				for (q = t->buckets[i]; q; q = q->link)
				{
					if (!set_member(s, q->member))
					{
						struct member *p;
						const void *member = q->member;
						int i = (*set->hash)(member) % set->size;
						NEW(p);
						p->member = member;
						p->link = set->buckets[i];
						set->buckets[i] = p;
						set->length++;
					}
				}
			}
		}
	
	return set;
	}
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_assert.h"

static int my_isblank(char c)
{
	return c == '\t' || c == ' ' || c == '\r' || c == '\n';
}

static char *sskip(char *s)
{
	while (my_isblank(*s))
		s++;

	return s;
}

static char *next_word(char *w, size_t wsize, char *s)
{
	size_t i;

	s = sskip(s);

	for (i = 0; i < wsize - 1; i++) {
		if (*s == 0 || my_isblank(s[i]))
			break;
		w[i] = s[i];
	}
	w[i] = 0;

	if (*s != 0 && !my_isblank(s[i]))
		printf("warning: '%s' truncated\n", w);

	return s + i;
}

struct sl_item {
	char *name;
	int is_replace;
};

static int cmp_sym(const void *p1_, const void *p2_)
{
	const struct sl_item *p1 = p1_, *p2 = p2_;
	const char *s1 = p1->name, *s2 = p2->name;
	int i;

	for (i = 0; ; i++) {
		if (s1[i] == s2[i])
			continue;

		if (s1[i] == 0) {
			if (s2[i] == 0 || s2[i] == '@')
				break;
		}

		if (s2[i] == 0 || s1[i] == '@')
			break;

		return s1[i] - s2[i];
	}

	return 0;
}

static int cmp_sym_sort(const void *p1_, const void *p2_)
{
	const struct sl_item *p1 = p1_, *p2 = p2_;
	const char *s1 = p1->name, *s2 = p2->name;
	int ret;
	
	ret = cmp_sym(p1_, p2_);
	if (ret == 0) {
		printf("%s: dupe sym: '%s' '%s'\n", __func__, s1, s2);
		exit(1);
	}
	return ret;
}

void read_list(struct sl_item **sl_in, int *cnt, int *alloc, FILE *f, int is_repl)
{
	struct sl_item *sl = *sl_in;
	int c = *cnt;
	char line[256];
	char word[256];
	char *r;

	while (fgets(line, sizeof(line), f) != NULL) {
		r = next_word(word, sizeof(word), line);
		if (r == word)
			continue;

		sl[c].name = strdup(word);
		sl[c].is_replace = is_repl;
		c++;

		if (c >= *alloc) {
			*alloc *= 2;
			sl = realloc(sl, *alloc * sizeof(sl[0]));
			my_assert_not(sl, NULL);
		}
	}

	*sl_in = sl;
	*cnt = c;
}

int main(int argc, char *argv[])
{
	struct sl_item *symlist, *sym, ssym;
	FILE *fout, *fin, *f;
	int symlist_alloc;
	int symlist_cnt;
	char line[256];
	char word[256];
	char word2[256];
	char word3[256];
	char word4[256];
	char *p;

	if (argc != 5) {
		// rmlist - prefix func with 'rm_', callsites with '_'
		// renlist - prefix func and callsites with 'rm_'
		printf("usage:\n%s <asmf_out> <asmf_in> <rmlist> <renlist>\n",
			argv[0]);
		return 1;
	}

	symlist_alloc = 16;
	symlist_cnt = 0;
	symlist = malloc(symlist_alloc * sizeof(symlist[0]));
	my_assert_not(symlist, NULL);

	f = fopen(argv[3], "r");
	my_assert_not(f, NULL);
	read_list(&symlist, &symlist_cnt, &symlist_alloc, f, 1);
	fclose(f);

	f = fopen(argv[4], "r");
	my_assert_not(f, NULL);
	read_list(&symlist, &symlist_cnt, &symlist_alloc, f, 0);
	fclose(f);

	qsort(symlist, symlist_cnt, sizeof(symlist[0]), cmp_sym_sort);

printf("symlist:\n");
int i;
for (i = 0; i < symlist_cnt; i++)
 printf("%d '%s'\n", symlist[i].is_replace, symlist[i].name);

	fin = fopen(argv[2], "r");
	my_assert_not(fin, NULL);

	fout = fopen(argv[1], "w");
	my_assert_not(fout, NULL);

	while (fgets(line, sizeof(line), fin))
	{
		p = sskip(line);
		if (*p == 0 || *p == ';')
			goto pass;

		p = sskip(next_word(word, sizeof(word), p));
		if (*p == 0 || *p == ';')
			goto pass; // need at least 2 words

		p = next_word(word2, sizeof(word2), p);

		if (!strcasecmp(word2, "proc") || !strcasecmp(word2, "endp")) {
			ssym.name = word;
			sym = bsearch(&ssym, symlist, symlist_cnt,
				sizeof(symlist[0]), cmp_sym);
			if (sym != NULL) {
				fprintf(fout, "rm_%s\t%s%s", word, word2, p);
				continue;
			}
		}

		if (!strcasecmp(word, "call") || !strcasecmp(word, "jmp")) {
			ssym.name = word2;
			sym = bsearch(&ssym, symlist, symlist_cnt,
				sizeof(symlist[0]), cmp_sym);
			if (sym != NULL) {
				fprintf(fout, "\t\t%s\t%s%s%s", word,
					sym->is_replace ? "_" : "rm_", word2, p);
				continue;
			}
		}

		p = sskip(p);
		if (*p == 0 || *p == ';')
			goto pass; // need at least 3 words

		p = next_word(word3, sizeof(word3), p);

		if (!strcasecmp(word, "dd") && !strcasecmp(word2, "offset")) {
			ssym.name = word3;
			sym = bsearch(&ssym, symlist, symlist_cnt,
				sizeof(symlist[0]), cmp_sym);
			if (sym != NULL) {
				fprintf(fout, "\t\tdd offset %s%s", word3, p);
				continue;
			}
		}

		p = sskip(p);
		if (*p == 0 || *p == ';')
			goto pass; // need at least 4 words

		p = next_word(word4, sizeof(word4), p);

		if (!strcasecmp(word2, "dd") && !strcasecmp(word3, "offset")) {
			ssym.name = word4;
			sym = bsearch(&ssym, symlist, symlist_cnt,
				sizeof(symlist[0]), cmp_sym);
			if (sym != NULL) {
				fprintf(fout, "%s\tdd offset %s%s%s", word,
					sym->is_replace ? "_" : "rm_", word4, p);
				continue;
			}
		}

pass:
		fwrite(line, 1, strlen(line), fout);
	}

	fclose(fin);
	fclose(fout);

	return 0;
}

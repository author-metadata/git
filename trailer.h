#ifndef TRAILER_H
#define TRAILER_H

#include "list.h"
#include "strbuf.h"

struct trailer_block;
struct trailer_conf;

enum trailer_where {
	WHERE_DEFAULT,
	WHERE_END,
	WHERE_AFTER,
	WHERE_BEFORE,
	WHERE_START
};
enum trailer_if_exists {
	EXISTS_DEFAULT,
	EXISTS_ADD_IF_DIFFERENT_NEIGHBOR,
	EXISTS_ADD_IF_DIFFERENT,
	EXISTS_ADD,
	EXISTS_REPLACE,
	EXISTS_DO_NOTHING
};
enum trailer_if_missing {
	MISSING_DEFAULT,
	MISSING_ADD,
	MISSING_DO_NOTHING
};

int trailer_set_where(enum trailer_where *item, const char *value);
int trailer_set_if_exists(enum trailer_if_exists *item, const char *value);
int trailer_set_if_missing(enum trailer_if_missing *item, const char *value);

/*
 * A list that represents newly-added trailers, such as those provided
 * with the --trailer command line option of git-interpret-trailers.
 */
struct new_trailer_item {
	struct list_head list;

	const char *text;

	enum trailer_where where;
	enum trailer_if_exists if_exists;
	enum trailer_if_missing if_missing;
};

void trailer_set_conf_where(enum trailer_where, struct trailer_conf *);
void trailer_set_conf_if_exists(enum trailer_if_exists, struct trailer_conf *);
void trailer_set_conf_if_missing(enum trailer_if_missing, struct trailer_conf *);

struct trailer_conf *new_trailer_conf(void);
void duplicate_trailer_conf(struct trailer_conf *dst,
			    const struct trailer_conf *src);
const char *trailer_default_separators(void);
void trailer_add_arg_item(struct list_head *arg_head, char *tok, char *val,
			  const struct trailer_conf *conf,
			  const struct new_trailer_item *new_trailer_item);

struct process_trailer_options {
	int in_place;
	int trim_empty;
	int only_trailers;
	int only_input;
	int unfold;
	int no_divider;
	int key_only;
	int value_only;
	const struct strbuf *separator;
	const struct strbuf *key_value_separator;
	int (*filter)(const struct strbuf *, void *);
	void *filter_data;
};

#define PROCESS_TRAILER_OPTIONS_INIT {0}

void parse_trailers_from_config(struct list_head *config_head);

void parse_trailers_from_command_line_args(struct list_head *arg_head,
					   struct list_head *new_trailer_head);

void process_trailers_lists(struct list_head *head,
			    struct list_head *arg_head);

ssize_t find_separator(const char *line, const char *separators);

void parse_trailer(const char *line, ssize_t separator_pos,
		   struct strbuf *tok, struct strbuf *val,
		   const struct trailer_conf **);

struct trailer_block *parse_trailers(const struct process_trailer_options *,
				     const char *str,
				     struct list_head *head);

size_t trailer_block_start(struct trailer_block *);
size_t trailer_block_end(struct trailer_block *);
int blank_line_before_trailer_block(struct trailer_block *);

void trailer_block_release(struct trailer_block *);

void trailer_config_init(void);
void format_trailers(const struct process_trailer_options *,
		     struct list_head *trailers,
		     struct strbuf *out);
void free_trailers(struct list_head *);
void free_new_trailers(struct list_head *);
void free_trailer_conf(struct trailer_conf *);

/*
 * Convenience function to format the trailers from the commit msg "msg" into
 * the strbuf "out". Reuses format_trailers() internally.
 */
void format_trailers_from_commit(const struct process_trailer_options *,
				 const char *msg,
				 struct strbuf *out);

/*
 * An interface for iterating over the trailers found in a particular commit
 * message. Use like:
 *
 *   struct trailer_iterator iter;
 *   trailer_iterator_init(&iter, msg);
 *   while (trailer_iterator_advance(&iter))
 *      ... do something with iter.key and iter.val ...
 *   trailer_iterator_release(&iter);
 */
struct trailer_iterator {
	/*
	 * Raw line (e.g., "foo: bar baz") before being parsed as a trailer
	 * key/val pair as part of a trailer block. A trailer block can be
	 * either 100% trailer lines, or mixed in with non-trailer lines (in
	 * which case at least 25% must be trailer lines).
	 */
	const char *raw;

	struct strbuf key;
	struct strbuf val;

	/* private */
	struct {
		struct trailer_block *trailer_block;
		size_t cur;
	} internal;
};

/*
 * Initialize "iter" in preparation for walking over the trailers in the commit
 * message "msg". The "msg" pointer must remain valid until the iterator is
 * released.
 *
 * After initializing, note that key/val will not yet point to any trailer.
 * Call advance() to parse the first one (if any).
 */
void trailer_iterator_init(struct trailer_iterator *iter, const char *msg);

/*
 * Advance to the next trailer of the iterator. Returns 0 if there is no such
 * trailer, and 1 otherwise. The key and value of the trailer can be
 * fetched from the iter->key and iter->value fields (which are valid
 * only until the next advance).
 */
int trailer_iterator_advance(struct trailer_iterator *iter);

/*
 * Release all resources associated with the trailer iteration.
 */
void trailer_iterator_release(struct trailer_iterator *iter);

#endif /* TRAILER_H */

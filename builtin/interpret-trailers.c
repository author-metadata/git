/*
 * Builtin "git interpret-trailers"
 *
 * Copyright (c) 2013, 2014 Christian Couder <chriscool@tuxfamily.org>
 *
 */

#include "builtin.h"
#include "gettext.h"
#include "parse-options.h"
#include "string-list.h"
#include "tempfile.h"
#include "trailer.h"
#include "config.h"

static const char * const git_interpret_trailers_usage[] = {
	N_("git interpret-trailers [--in-place] [--trim-empty]\n"
	   "                       [(--trailer (<key>|<keyAlias>)[(=|:)<value>])...]\n"
	   "                       [--parse] [<file>...]"),
	NULL
};

static enum trailer_where where;
static enum trailer_if_exists if_exists;
static enum trailer_if_missing if_missing;

static int option_parse_where(const struct option *opt,
			      const char *arg, int unset UNUSED)
{
	/* unset implies NULL arg, which is handled in our helper */
	return trailer_set_where(arg, opt->value);
}

static int option_parse_if_exists(const struct option *opt,
				  const char *arg, int unset UNUSED)
{
	/* unset implies NULL arg, which is handled in our helper */
	return trailer_set_if_exists(arg, opt->value);
}

static int option_parse_if_missing(const struct option *opt,
				   const char *arg, int unset UNUSED)
{
	/* unset implies NULL arg, which is handled in our helper */
	return trailer_set_if_missing(arg, opt->value);
}

static int option_parse_trailer(const struct option *opt,
				   const char *arg, int unset)
{
	struct list_head *trailers = opt->value;
	struct strbuf tok = STRBUF_INIT;
	struct strbuf val = STRBUF_INIT;
	const struct trailer_conf *conf;
	ssize_t separator_pos;
	static char *cl_separators;

	if (unset) {
		free_new_trailers(trailers);
		return 0;
	}

	if (!arg)
		return -1;

	/*
	 * In command-line arguments, '=' is accepted (in addition to the
	 * separators that are defined).
	 */
	cl_separators = xstrfmt("=%s", trailer_default_separators());
	separator_pos = find_separator(arg, cl_separators);
	free(cl_separators);

	if (separator_pos == 0) {
		struct strbuf sb = STRBUF_INIT;
		strbuf_addstr(&sb, arg);
		strbuf_trim(&sb);
		error(_("empty trailer token in trailer '%.*s'"),
			(int) sb.len, sb.buf);
		strbuf_release(&sb);
	} else {
		struct trailer_conf *conf_current = new_trailer_conf();
		parse_trailer(arg, separator_pos, &tok, &val, &conf);
		duplicate_trailer_conf(conf_current, conf);

		/*
		 * Override conf_current with settings specified via CLI flags.
		 */
		if (where != WHERE_DEFAULT)
			trailer_set_conf_where(where, conf_current);
		if (if_exists != EXISTS_DEFAULT)
			trailer_set_conf_if_exists(if_exists, conf_current);
		if (if_missing != MISSING_DEFAULT)
			trailer_set_conf_if_missing(if_missing, conf_current);

		trailer_add_arg_item(trailers,
				     strbuf_detach(&tok, NULL),
				     strbuf_detach(&val, NULL),
				     conf_current);
		free_trailer_conf(conf_current);
	}

	return 0;
}

static int parse_opt_parse(const struct option *opt, const char *arg,
			   int unset)
{
	struct process_trailer_options *v = opt->value;
	v->only_trailers = 1;
	v->only_input = 1;
	v->unfold = 1;
	BUG_ON_OPT_NEG(unset);
	BUG_ON_OPT_ARG(arg);
	return 0;
}

static struct tempfile *trailers_tempfile;

static FILE *create_in_place_tempfile(const char *file)
{
	struct stat st;
	struct strbuf filename_template = STRBUF_INIT;
	const char *tail;
	FILE *outfile;

	if (stat(file, &st))
		die_errno(_("could not stat %s"), file);
	if (!S_ISREG(st.st_mode))
		die(_("file %s is not a regular file"), file);
	if (!(st.st_mode & S_IWUSR))
		die(_("file %s is not writable by user"), file);

	/* Create temporary file in the same directory as the original */
	tail = strrchr(file, '/');
	if (tail)
		strbuf_add(&filename_template, file, tail - file + 1);
	strbuf_addstr(&filename_template, "git-interpret-trailers-XXXXXX");

	trailers_tempfile = xmks_tempfile_m(filename_template.buf, st.st_mode);
	strbuf_release(&filename_template);
	outfile = fdopen_tempfile(trailers_tempfile, "w");
	if (!outfile)
		die_errno(_("could not open temporary file"));

	return outfile;
}

static void read_input_file(struct strbuf *sb, const char *file)
{
	if (file) {
		if (strbuf_read_file(sb, file, 0) < 0)
			die_errno(_("could not read input file '%s'"), file);
	} else {
		if (strbuf_read(sb, fileno(stdin), 0) < 0)
			die_errno(_("could not read from stdin"));
	}
}

static void interpret_trailers(const struct process_trailer_options *opts,
			       struct list_head *new_trailer_head,
			       const char *file)
{
	LIST_HEAD(head);
	struct strbuf sb = STRBUF_INIT;
	struct strbuf tb = STRBUF_INIT;
	struct trailer_block *trailer_block;
	FILE *outfile = stdout;

	read_input_file(&sb, file);

	if (opts->in_place)
		outfile = create_in_place_tempfile(file);

	trailer_block = parse_trailers(opts, sb.buf, &head);

	/* Print the lines before the trailer block */
	if (!opts->only_trailers)
		fwrite(sb.buf, 1, trailer_block_start(trailer_block), outfile);

	if (!opts->only_trailers && !blank_line_before_trailer_block(trailer_block))
		fprintf(outfile, "\n");

	if (!opts->only_input)
		process_trailers_lists(&head, new_trailer_head);

	/* Print trailer block. */
	format_trailers(opts, &head, &tb);
	free_trailers(&head);
	fwrite(tb.buf, 1, tb.len, outfile);
	strbuf_release(&tb);

	/* Print the lines after the trailer block as is. */
	if (!opts->only_trailers)
		fwrite(sb.buf + trailer_block_end(trailer_block), 1,
		       sb.len - trailer_block_end(trailer_block), outfile);
	trailer_block_release(trailer_block);

	if (opts->in_place)
		if (rename_tempfile(&trailers_tempfile, file))
			die_errno(_("could not rename temporary file to %s"), file);

	strbuf_release(&sb);
}

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	struct process_trailer_options opts = PROCESS_TRAILER_OPTIONS_INIT;
	LIST_HEAD(configured_trailers);
	LIST_HEAD(trailers);

	struct option options[] = {
		OPT_BOOL(0, "in-place", &opts.in_place, N_("edit files in place")),
		OPT_BOOL(0, "trim-empty", &opts.trim_empty, N_("trim empty trailers")),

		OPT_CALLBACK(0, "where", &where, N_("placement"),
			     N_("where to place the new trailer"), option_parse_where),
		OPT_CALLBACK(0, "if-exists", &if_exists, N_("action"),
			     N_("action if trailer already exists"), option_parse_if_exists),
		OPT_CALLBACK(0, "if-missing", &if_missing, N_("action"),
			     N_("action if trailer is missing"), option_parse_if_missing),

		OPT_BOOL(0, "only-trailers", &opts.only_trailers, N_("output only the trailers")),
		OPT_BOOL(0, "only-input", &opts.only_input, N_("do not apply trailer.* configuration variables")),
		OPT_BOOL(0, "unfold", &opts.unfold, N_("reformat multiline trailer values as single-line values")),
		OPT_CALLBACK_F(0, "parse", &opts, NULL, N_("alias for --only-trailers --only-input --unfold"),
			PARSE_OPT_NOARG | PARSE_OPT_NONEG, parse_opt_parse),
		OPT_BOOL(0, "no-divider", &opts.no_divider, N_("do not treat \"---\" as the end of input")),
		OPT_CALLBACK(0, "trailer", &trailers, N_("trailer"),
				N_("trailer(s) to add"), option_parse_trailer),
		OPT_END()
	};

	git_config(git_default_config, NULL);
	trailer_config_init();

	if (!opts.only_input)
		parse_trailers_from_config(&configured_trailers);

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	if (opts.only_input && !list_empty(&trailers))
		usage_msg_opt(
			_("--trailer with --only-input does not make sense"),
			git_interpret_trailers_usage,
			options);

	list_splice(&configured_trailers, &trailers);

	if (argc) {
		int i;
		for (i = 0; i < argc; i++)
			interpret_trailers(&opts, &trailers, argv[i]);
	} else {
		if (opts.in_place)
			die(_("no input file given for in-place editing"));
		interpret_trailers(&opts, &trailers, NULL);
	}

	free_new_trailers(&trailers);

	return 0;
}

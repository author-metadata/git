	echo 1 >file &&
	echo 2 >file &&
	echo 3 >file &&
	echo 1 >file2 &&
	echo 1 >HEAD &&
	cat >expect <<-EOF &&
	?? actual
	?? expect
	EOF

	one_blob=$(echo 1 | git hash-object --stdin) &&
	tracked=$(git rev-parse --short "$one_blob") &&
	untracked_blob=$(echo untracked | git hash-object --stdin) &&
	untracked=$(git rev-parse --short "$untracked_blob") &&
	cat >expect.diff <<-EOF &&
	diff --git a/HEAD b/HEAD
	new file mode 100644
	index 0000000..$tracked
	--- /dev/null
	+++ b/HEAD
	@@ -0,0 +1 @@
	+1
	diff --git a/file2 b/file2
	new file mode 100644
	index 0000000..$tracked
	--- /dev/null
	+++ b/file2
	@@ -0,0 +1 @@
	+1
	diff --git a/untracked/untracked b/untracked/untracked
	new file mode 100644
	index 0000000..$untracked
	--- /dev/null
	+++ b/untracked/untracked
	@@ -0,0 +1 @@
	+untracked
	EOF
	cat >expect.lstree <<-EOF &&
	HEAD
	file2
	untracked
	EOF

test_expect_success 'clean up untracked/untracked file to prepare for next tests' '
	git clean --force --quiet
'
	cat >expect <<-EOF &&
	 M file
	?? HEAD
	?? actual
	?? expect
	?? file2
	?? untracked/
	EOF

	echo 1 >expect_file2 &&
	test_cmp expect_file2 file2 &&
	echo untracked >untracked_expect &&
	test_cmp untracked_expect untracked/untracked
test_expect_success 'clean up untracked/ directory to prepare for next tests' '
	git clean --force --quiet -d
'
	echo 4 >file3 &&
	four_blob=$(echo 4 | git hash-object --stdin) &&
	blob=$(git rev-parse --short "$four_blob") &&
	cat >expect <<-EOF &&
	diff --git a/file3 b/file3
	new file mode 100644
	index 0000000..$blob
	--- /dev/null
	+++ b/file3
	@@ -0,0 +1 @@
	+4
	EOF

	test_when_finished "git reset" &&
	echo 1 >file5 &&
	git stash save --include-untracked --quiet >.git/stash-output.out 2>&1 &&
	echo 1 >expect &&
	test_when_finished "rm -f expect" &&
	cat >.gitignore <<-EOF &&
	.gitignore
	ignored
	ignored.d/
	EOF

	echo ignored >ignored &&
	test_file_not_empty ignored &&
	test_file_not_empty ignored.d/untracked &&
	test_file_not_empty .gitignore
	echo 4 >file4 &&
	test_file_not_empty ignored &&
	test_file_not_empty ignored.d/untracked &&
	test_file_not_empty .gitignore
	cat >.gitignore <<-EOF &&
	ignored
	ignored.d/*
	EOF

	echo "!ignored.d/foo" >>.gitignore &&
	test_cmp expect actual
test_expect_success 'stash show --include-untracked shows untracked files' '
	git reset --hard &&
	git clean -xf &&
	>untracked &&
	>tracked &&
	git add tracked &&
	empty_blob_oid=$(git rev-parse --short :tracked) &&
	git stash -u &&

	cat >expect <<-EOF &&
	 tracked   | 0
	 untracked | 0
	 2 files changed, 0 insertions(+), 0 deletions(-)
	EOF
	git stash show --include-untracked >actual &&
	test_cmp expect actual &&
	git stash show -u >actual &&
	test_cmp expect actual &&
	git stash show --no-include-untracked --include-untracked >actual &&
	test_cmp expect actual &&
	git stash show --only-untracked --include-untracked >actual &&
	test_cmp expect actual &&
	git -c stash.showIncludeUntracked=true stash show >actual &&
	test_cmp expect actual &&

	cat >expect <<-EOF &&
	diff --git a/tracked b/tracked
	new file mode 100644
	index 0000000..$empty_blob_oid
	diff --git a/untracked b/untracked
	new file mode 100644
	index 0000000..$empty_blob_oid
	EOF
	git stash show -p --include-untracked >actual &&
	test_cmp expect actual &&
	git stash show --include-untracked -p >actual &&
	test_cmp expect actual &&
	git -c stash.showIncludeUntracked=true stash show -p >actual &&
	test_cmp expect actual
'

test_expect_success 'stash show --only-untracked only shows untracked files' '
	git reset --hard &&
	git clean -xf &&
	>untracked &&
	>tracked &&
	git add tracked &&
	empty_blob_oid=$(git rev-parse --short :tracked) &&
	git stash -u &&

	cat >expect <<-EOF &&
	 untracked | 0
	 1 file changed, 0 insertions(+), 0 deletions(-)
	EOF
	git stash show --only-untracked >actual &&
	test_cmp expect actual &&
	git stash show --no-include-untracked --only-untracked >actual &&
	test_cmp expect actual &&
	git stash show --include-untracked --only-untracked >actual &&
	test_cmp expect actual &&

	cat >expect <<-EOF &&
	diff --git a/untracked b/untracked
	new file mode 100644
	index 0000000..$empty_blob_oid
	EOF
	git stash show -p --only-untracked >actual &&
	test_cmp expect actual &&
	git stash show --only-untracked -p >actual &&
	test_cmp expect actual
'

test_expect_success 'stash show --no-include-untracked cancels --{include,only}-untracked' '
	git reset --hard &&
	git clean -xf &&
	>untracked &&
	>tracked &&
	git add tracked &&
	git stash -u &&

	cat >expect <<-EOF &&
	 tracked | 0
	 1 file changed, 0 insertions(+), 0 deletions(-)
	EOF
	git stash show --only-untracked --no-include-untracked >actual &&
	test_cmp expect actual &&
	git stash show --include-untracked --no-include-untracked >actual &&
	test_cmp expect actual
'

test_expect_success 'stash show --include-untracked errors on duplicate files' '
	git reset --hard &&
	git clean -xf &&
	>tracked &&
	git add tracked &&
	tree=$(git write-tree) &&
	i_commit=$(git commit-tree -p HEAD -m "index on any-branch" "$tree") &&
	test_when_finished "rm -f untracked_index" &&
	u_commit=$(
		GIT_INDEX_FILE="untracked_index" &&
		export GIT_INDEX_FILE &&
		git update-index --add tracked &&
		u_tree=$(git write-tree) &&
		git commit-tree -m "untracked files on any-branch" "$u_tree"
	) &&
	w_commit=$(git commit-tree -p HEAD -p "$i_commit" -p "$u_commit" -m "WIP on any-branch" "$tree") &&
	test_must_fail git stash show --include-untracked "$w_commit" 2>err &&
	test_i18ngrep "worktree and untracked commit have duplicate entries: tracked" err
'

test_expect_success 'stash show --{include,only}-untracked on stashes without untracked entries' '
	git reset --hard &&
	git clean -xf &&
	>tracked &&
	git add tracked &&
	git stash &&

	git stash show >expect &&
	git stash show --include-untracked >actual &&
	test_cmp expect actual &&

	git stash show --only-untracked >actual &&
	test_must_be_empty actual
'

test_expect_success 'stash -u ignores sub-repository' '
	test_when_finished "rm -rf sub-repo" &&
	git init sub-repo &&
	git stash -u
'
